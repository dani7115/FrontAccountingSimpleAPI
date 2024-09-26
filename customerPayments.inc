<?php
/**
 * Author : Danish Naseem <dani.115115@gmail.com>
 */
$page_security = 'ST_CUSTPAYMENT';
$path_to_root = "../..";
include_once($path_to_root . "/includes/ui/allocation_cart.inc");
//include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/banking.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/reporting/includes/reporting.inc");


//----------------------------------------------------------------------------------------------

function customerPayments_add()
{
    c_log('Step 1');
    if (can_process()) {
        c_log('step can_process ');
        new_doc_date($_POST['DateBanked']);

        $_SESSION['alloc'] = new allocation(ST_CUSTPAYMENT, 0, get_post('customer_id'));

        $payment_no = write_customer_payment($_SESSION['alloc']->trans_no, $_POST['customer_id'], $_POST['BranchID'],
            $_POST['bank_account'], $_POST['DateBanked'], $_POST['ref'],
            input_num('amount'), input_num('discount'), $_POST['memo_'], 0, input_num('charge'), input_num('bank_amount', input_num('amount')), $_POST['dimension_id'], $_POST['dimension2_id']);

        c_log('step $payment_no ');

        $_SESSION['alloc']->trans_no = $payment_no;
        $_SESSION['alloc']->date_ = $_POST['DateBanked'];
        $_SESSION['alloc']->write();

        c_log('step AFTER write() ');

        unset($_SESSION['alloc']);

        if($payment_no){
            api_success_response(sprintf(_("Payment # %d has been entered."), $payment_no));
        }else{
            api_error(412, 'Failed to add Payment invoice.');
        }

    }else{
        c_log('step cannot process ');
        api_error(500, 'Payment data is invalid.');
    }
}

function can_process()
{
    global $Refs;

    if (!get_post('customer_id'))
    {
        display_error(_("There is no customer selected."));
        set_focus('customer_id');
        return false;
    }

    if (!get_post('BranchID'))
    {
        display_error(_("This customer has no branch defined."));
        set_focus('BranchID');
        return false;
    }

    if (!isset($_POST['DateBanked']) || !is_date($_POST['DateBanked'])) {
        display_error(_("The entered date is invalid. Please enter a valid date for the payment."));
        set_focus('DateBanked');
        return false;
    } elseif (!is_date_in_fiscalyear($_POST['DateBanked'])) {
        display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
        set_focus('DateBanked');
        return false;
    }

    if (!check_reference($_POST['ref'], ST_CUSTPAYMENT, @$_POST['trans_no'])) {
        set_focus('ref');
        return false;
    }

    if (!check_num('amount', 0)) {
        display_error(_("The entered amount is invalid or negative and cannot be processed."));
        set_focus('amount');
        return false;
    }

    if (isset($_POST['charge']) && (!check_num('charge', 0) || $_POST['charge'] == $_POST['amount'])) {
        display_error(_("The entered amount is invalid or negative and cannot be processed."));
        set_focus('charge');
        return false;
    }
    if (isset($_POST['charge']) && input_num('charge') > 0) {
        $charge_acct = get_bank_charge_account($_POST['bank_account']);
        if (get_gl_account($charge_acct) == false) {
            display_error(_("The Bank Charge Account has not been set in System and General GL Setup."));
            set_focus('charge');
            return false;
        }
    }

    if (@$_POST['discount'] == "")
    {
        $_POST['discount'] = 0;
    }

    if (!check_num('discount')) {
        display_error(_("The entered discount is not a valid number."));
        set_focus('discount');
        return false;
    }

    if (input_num('amount') <= 0) {
        display_error(_("The balance of the amount and discount is zero or negative. Please enter valid amounts."));
        set_focus('discount');
        return false;
    }

    if (isset($_POST['bank_amount']) && input_num('bank_amount')<=0)
    {
        display_error(_("The entered payment amount is zero or negative."));
        set_focus('bank_amount');
        return false;
    }

    if (!db_has_currency_rates(get_customer_currency($_POST['customer_id']), $_POST['DateBanked'], true))
        return false;

    $_SESSION['alloc']->amount = input_num('amount');

    if (isset($_POST["TotalNumberOfAllocs"]))
        return check_allocations();
    else
        return true;
}


function customerPayments_delete(){
    $resp=['status'=>true,'msg'=>''];
    try {
        $_POST['filterType']=ST_CUSTPAYMENT;
        if (check_valid_entries()==true)
        {
            $void_entry = get_voided_entry($_POST['filterType'], $_POST['trans_no']);
            if ($void_entry != null)
            {
                $resp['status']=false;
                $resp['error_code']=500;
                $resp['msg']="The selected transaction has already been voided.";

            } else {
                $msg = void_transaction($_POST['filterType'], $_POST['trans_no'], $_POST['date_'], $_POST['memo_']);
                if (!$msg) {
                    $resp['status'] = true;
                } else {
                    $resp['status'] = false;
                    $resp['error_code'] = 500;
                    $resp['msg'] = $msg;
                }
            }
        }
    } catch (Exception $e) {
        error_log($e->getMessage(), 3, "/var/tmp/sales_cancel.log");
        $resp['msg']='Could not cancel invoice. ';
        return;
    }
    if($resp['status']){
        api_success_response($resp);
    }else{
        api_error($resp['error_code'], $resp['msg']);
    }

}

function check_valid_entries()
{
    $check=array('status'=>true,'msg');

    if (is_closed_trans($_POST['filterType'],$_POST['trans_no']))
    {
        $check['msg'] = "The selected transaction was closed for edition and cannot be voided.";
        $check['status'] = false;
    }
    if (!is_date($_POST['date_']))
    {
        $check['msg'] = "The entered date is invalid.";
        $check['status'] = false;
    }
    if (!is_date_in_fiscalyear($_POST['date_']))
    {
        $check['msg'] ="The entered date is out of fiscal year or is closed for further data entry.";
        $check['status'] = false;
    }

    if (!is_numeric($_POST['trans_no']) OR $_POST['trans_no'] <= 0)
    {
        $check['msg'] ="The transaction number is expected to be numeric and greater than zero.";
        $check['status'] = false;
    }

    return $check;
}


function getAvailableCustomerPayments()
{
    $resp=['status'=>true,'msg'=>'success'];
    try {

        $customer_id = $_POST['customer_id'];

        $sql = "SELECT
		trans.type,
		trans.trans_no,
		trans.reference,
		trans.tran_date,
		debtor.name AS DebtorName, 
		debtor.curr_code,
		ov_amount+ov_gst+ov_freight+ov_freight_tax+ov_discount AS Total,
		trans.alloc,
		trans.due_date,
		debtor.address,
		trans.version,
		round(abs(ov_amount+ov_gst+ov_freight+ov_freight_tax+ov_discount-alloc),6) <= 0 AS settled,
		trans.debtor_no

	 FROM "
            . TB_PREF . "debtor_trans as trans, "
            . TB_PREF . "debtors_master as debtor"
            . " WHERE trans.debtor_no=debtor.debtor_no
		AND (((type=" . ST_CUSTPAYMENT . " OR type=" . ST_BANKDEPOSIT . ") AND (trans.ov_amount > 0))
		 OR (type=" . ST_CUSTCREDIT . " AND (ov_amount+ov_gst+ov_freight+ov_freight_tax+ov_discount)>0)
		 OR (type=" . ST_JOURNAL . " AND (trans.ov_amount < 0)))";


        $sql .= " AND trans.debtor_no = " . db_escape($customer_id);

        $result = db_query($sql);
        $resp['data'] = [];

        if ($result) {
            while ($row = db_fetch_assoc($result)) {
                $resp['data'][] = $row;
            }
        }
    } catch (\Exception $e) {
        $resp['status']=false;
        $resp['msg']=$e->getMessage();
    }

    api_success_response(json_encode($resp));

}

function getCustomerSalesForAllocate()
{
    $resp=['status'=>true,'msg'=>'success'];
    try {
        $allocation=new allocation($_POST['trans_type'],$_POST['trans_no']);
        $resp['data']=$allocation->allocs;
    } catch (\Exception $e) {
        $resp['status']=false;
        $resp['msg']=$e->getMessage();
    }

    api_success_response(json_encode($resp));

}

function customerPayments_allocate()
{
    $resp=['status'=>true,'msg'=>'success'];
    try {
        $_SESSION['alloc']=new allocation($_POST['trans']['type'],$_POST['trans']['trans_no']);
        $_POST=$_POST['alloc_data'];
        $SysPrefs = new sys_prefs();

        $total_allocated = 0;


        for ($counter = 0; $counter < sizeof($_POST); $counter++)
        {
            if (!isset($_POST['amount'.$counter])) continue;
            if (!check_num('amount' . $counter, 0))
            {
                $resp['status']=false;
                $resp['msg']="The entry for one or more amounts is invalid or negative. ( ".$_POST['amount'.$counter]."";
                break;
            }

            /* Now check to see that the AllocAmt is no greater than the
           amount left to be allocated against the transaction under review;
           skip check if no allocation is set to avoid deadlock on mistakenly overallocated transactions*/
            $allocated = input_num('amount' . $counter);
            if ($allocated && ($allocated > input_num('un_allocated' . $counter)))
            {
                $resp['status']=false;
                $resp['msg']="At least one transaction is overallocated. ( ".$_POST['amount'.$counter]."";
                break;
            }

            if ($_SESSION['alloc']->person_type == PT_CUSTOMER) {
                if ($_SESSION['alloc']->allocs[$counter]->type == ST_SALESORDER)
                    $trans = get_sales_order_header($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type);
                else
                    $trans = get_customer_trans($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type,$_SESSION['alloc']->person_id);

                if ($trans['debtor_no'] != $_SESSION['alloc']->person_id) {
                    $resp['status']=false;
                    $resp['msg']="Allocated transaction allocated is not related to company selected. ( ".$_POST['amount'.$counter]."";
                    break;
                }
            } elseif ($_SESSION['alloc']->person_type == PT_SUPPLIER) {
                if ($_SESSION['alloc']->allocs[$counter]->type == ST_PURCHORDER)
                    $trans = get_po($_SESSION['alloc']->allocs[$counter]->type_no);
                else
                    $trans = get_supp_trans($_SESSION['alloc']->allocs[$counter]->type_no, $_SESSION['alloc']->allocs[$counter]->type,  $_SESSION['alloc']->person_id);
                if ($trans['supplier_id'] != $_SESSION['alloc']->person_id) {
                    $resp['status']=false;
                    $resp['msg']="Allocated transaction allocated is not related to company selected. ( ".$_POST['amount'.$counter]." )";
                    break;
                }
            }

            $_SESSION['alloc']->allocs[$counter]->current_allocated = input_num('amount' . $counter);

            $total_allocated += input_num('amount' . $counter);
        }

        $amount = abs($_SESSION['alloc']->amount);

        if ($total_allocated - ($amount + input_num('discount'))  > $SysPrefs->allocation_settled_allowance())
        {
            $resp['status']=false;
            $resp['msg']="These allocations cannot be processed because the amount allocated is more than the total amount left to allocate";
        }


        if ($resp['status']) {
            $_SESSION['alloc']->write();
        }


    } catch (\Exception $e) {
        $resp['status']=false;
        $resp['msg']=$e->getMessage();
    }

    api_success_response(json_encode($resp));

}
