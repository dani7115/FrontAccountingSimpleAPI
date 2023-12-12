<?php
$page_security = 'SA_GLTRANSVIEW';
$path_to_root = "../..";

include($path_to_root . "/includes/db_pager.inc");

include_once($path_to_root . "/admin/db/fiscalyears_db.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/gl/includes/gl_db.inc");

if (!isset($_POST["amount_min"]))
    $_POST["amount_min"] = price_format(0);
if (!isset($_POST["amount_max"]))
    $_POST["amount_max"] = price_format(0);

function inquiry__2()
{
    $resp=['status'=>true,'msg'=>'success','data_list'=>[]];
    global $path_to_root, $systypes_array;

    if (!isset($_POST["account"]))
        $_POST["account"] = null;

    $act_name = $_POST["account"] ? get_gl_account_name($_POST["account"]) : "";
    $dim = get_company_pref('use_dimension');

    /*Now get the transactions  */
    if (!isset($_POST['Dimension']))
        $_POST['Dimension'] = 0;
    if (!isset($_POST['Dimension2']))
        $_POST['Dimension2'] = 0;
    $result = get_gl_transactions($_POST['TransFromDate'], $_POST['TransToDate'], -1,
        $_POST["account"], $_POST['Dimension'], $_POST['Dimension2'], null,
        input_num('amount_min'), input_num('amount_max'), null, null, $_POST['Memo']);

    $colspan = ($dim == 2 ? "7" : ($dim == 1 ? "6" : "5"));

    if ($_POST["account"] != null)
        display_heading($_POST["account"]. "&nbsp;&nbsp;&nbsp;".$act_name);

    // Only show balances if an account is specified AND we're not filtering by amounts
    $show_balances = $_POST["account"] != null &&
        input_num("amount_min") == 0 &&
        input_num("amount_max") == 0;

    start_table(TABLESTYLE);

    $first_cols = array(_("Type"), _("#"), _("Reference"), _("Date"));

    if ($_POST["account"] == null)
        $account_col = array(_("Account"));
    else
        $account_col = array();

    if ($dim == 2)
        $dim_cols = array(_("Dimension")." 1", _("Dimension")." 2");
    elseif ($dim == 1)
        $dim_cols = array(_("Dimension"));
    else
        $dim_cols = array();

    if ($show_balances)
        $remaining_cols = array(_("Person/Item"), _("Debit"), _("Credit"), _("Balance"), _("Memo"), "");
    else
        $remaining_cols = array(_("Person/Item"), _("Debit"), _("Credit"), _("Memo"), "");

    $th = array_merge($first_cols, $account_col, $dim_cols, $remaining_cols);

    table_header($th);
    if ($_POST["account"] != null && is_account_balancesheet($_POST["account"]))
        $begin = "";
    else
    {
        $begin = get_fiscalyear_begin_for_date($_POST['TransFromDate']);
        if (date1_greater_date2($begin, $_POST['TransFromDate']))
            $begin = $_POST['TransFromDate'];
        $begin = add_days($begin, -1);
    }

    $bfw = 0;
    if ($show_balances) {
        $bfw = get_gl_balance_from_to($begin, $_POST['TransFromDate'], $_POST["account"], $_POST['Dimension'], $_POST['Dimension2']);
        start_row("class='inquirybg'");
        label_cell("<b>"._("Opening Balance")." - ".$_POST['TransFromDate']."</b>", "colspan=$colspan");
        display_debit_or_credit_cells($bfw, true);
        label_cell("");
        label_cell("");
        end_row();
    }

    $running_total = $bfw;
    $j = 1;
    $k = 0; //row colour counter

    while ($myrow = db_fetch($result))
    {

        alt_table_row_color($k);

        $running_total += $myrow["amount"];

        $resp['data_list'][]=$myrow;

        $trandate = sql2date($myrow["tran_date"]);

        label_cell($systypes_array[$myrow["type"]]);
        label_cell(get_gl_view_str($myrow["type"], $myrow["type_no"], $myrow["type_no"], true));
        label_cell(get_trans_view_str($myrow["type"],$myrow["type_no"],$myrow['reference']));
        label_cell($trandate);

        if ($_POST["account"] == null)
            label_cell($myrow["account"] . ' ' . get_gl_account_name($myrow["account"]));

        if ($dim >= 1)
            label_cell(get_dimension_string($myrow['dimension_id'], true));
        if ($dim > 1)
            label_cell(get_dimension_string($myrow['dimension2_id'], true));
        label_cell(payment_person_name($myrow["person_type_id"],$myrow["person_id"]));
        display_debit_or_credit_cells($myrow["amount"]);
        if ($show_balances)
            amount_cell($running_total);
        if ($myrow['memo_'] == "")
            $myrow['memo_'] = get_comments_string($myrow['type'], $myrow['type_no']);
        label_cell($myrow['memo_']);
        if ($myrow["type"] == ST_JOURNAL)
            echo "<td>" . trans_editor_link( $myrow["type"], $myrow["type_no"]) . "</td>";
        else
            label_cell("");
        end_row();

        $j++;
        if ($j == 12)
        {
            $j = 1;
            table_header($th);
        }
    }
    //end of while loop

    $resp['ending_balance']=$running_total;

    if ($show_balances) {
        start_row("class='inquirybg'");
        label_cell("<b>" . _("Ending Balance") ." - ".$_POST['TransToDate']. "</b>", "colspan=$colspan");
        display_debit_or_credit_cells($running_total, true);
        label_cell("");
        label_cell("");
        end_row();
    }

    end_table(2);
   // if (db_num_rows($result) == 0)
      //  display_note(_("No general ledger transactions have been created for the specified criteria."), 0, 1);

    api_success_response(json_encode($resp));
}

function inquiry()
{
    $resp=['status'=>true,'msg'=>'success','data_list'=>[]];
    try {

        if (!isset($_POST["account"]))
            $_POST["account"] = null;
        if (!isset($_POST['Dimension']))
            $_POST['Dimension'] = 0;
        if (!isset($_POST['Dimension2']))
            $_POST['Dimension2'] = 0;

        $result = get_gl_transactions($_POST['TransFromDate'], $_POST['TransToDate'], -1,
            $_POST["account"], $_POST['Dimension'], $_POST['Dimension2'], null,
            input_num('amount_min'), input_num('amount_max'), null, null, $_POST['Memo']);


        // Only show balances if an account is specified AND we're not filtering by amounts
        $show_balances = $_POST["account"] != null &&
            input_num("amount_min") == 0 &&
            input_num("amount_max") == 0;


        if ($_POST["account"] != null && is_account_balancesheet($_POST["account"]))
            $begin = "";
        else {
            $begin = get_fiscalyear_begin_for_date($_POST['TransFromDate']);
            if (date1_greater_date2($begin, $_POST['TransFromDate']))
                $begin = $_POST['TransFromDate'];
            $begin = add_days($begin, -1);
        }


        $bfw = 0;
        if ($show_balances) {
            $bfw = get_gl_balance_from_to($begin, $_POST['TransFromDate'], $_POST["account"], $_POST['Dimension'], $_POST['Dimension2']);
        }

        $running_total = $bfw;

        while ($myrow = db_fetch($result)) {

            $running_total += $myrow["amount"];

            if ($_POST["account"] == null)
                $myrow["account"] = get_gl_account_name($myrow["account"]);

            if(isset($_POST['list']))
                $resp['data_list'][]=$myrow;
        }

        $resp['ending_balance']=round2($running_total, user_price_dec());

    } catch (\Exception $e) {
        $resp['status']=false;
        $resp['msg']=$e->getMessage();
    }

    api_success_response(json_encode($resp));
}


function inquiry__()
{
    $resp=['status'=>true,'msg'=>'success','data_list'=>[]];
    try {

        if (!isset($_POST["account"]))
            $_POST["account"] = null;

        $act_name = $_POST["account"] ? get_gl_account_name($_POST["account"]) : "";

        /*Now get the transactions  */
        if (!isset($_POST['Dimension']))
            $_POST['Dimension'] = 0;
        if (!isset($_POST['Dimension2']))
            $_POST['Dimension2'] = 0;
           $result = get_gl_transactions($_POST['TransFromDate'], $_POST['TransToDate'], -1,
            $_POST["account"], $_POST['Dimension'], $_POST['Dimension2'], null,
            input_num('amount_min'), input_num('amount_max'), null, null, $_POST['Memo']);


        if ($_POST["account"] != null)
            $_POST["account"]=$_POST["account"]. "&nbsp;&nbsp;&nbsp;".$act_name;

        // Only show balances if an account is specified AND we're not filtering by amounts
        $show_balances = $_POST["account"] != null &&
            input_num("amount_min") == 0 &&
            input_num("amount_max") == 0;


        if ($_POST["account"] != null && is_account_balancesheet($_POST["account"]))
            $begin = "";
        else
        {
            $begin = get_fiscalyear_begin_for_date($_POST['TransFromDate']);
            if (date1_greater_date2($begin, $_POST['TransFromDate']))
                $begin = $_POST['TransFromDate'];
            $begin = add_days($begin, -1);
        }

        $bfw = 0;
        if ($show_balances) {
            $bfw = get_gl_balance_from_to($begin, $_POST['TransFromDate'], $_POST["account"], $_POST['Dimension'], $_POST['Dimension2']);
        }

        $running_total = $bfw;

        while ($myrow = db_fetch($result))
        {
            $running_total += $myrow["amount"];

            if ($_POST["account"] == null)
                $myrow["account"] = get_gl_account_name($myrow["account"]);

            if(isset($_POST['list']))
                $resp['data_list'][]=$myrow;


        }

     /*   if ($show_balances) {
            $resp['ending_balance']=display_debit_or_credit_cells_($running_total);
        }*/

         $resp['ending_balance']=$running_total;


    } catch (\Exception $e) {
        $resp['status']=false;
        $resp['msg']=$e->getMessage();
    }

    api_success_response(json_encode($resp));

}

function display_debit_or_credit_cells_($value)
{
    $value = round2($value, user_price_dec());
    if ($value >= 0)
    {
        return $value;
    }
    elseif ($value < 0)
    {
        return abs($value);
    }
}