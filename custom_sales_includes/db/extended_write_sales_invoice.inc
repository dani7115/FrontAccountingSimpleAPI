<?php

//-----------------------------------------------------------------------------
//	Add or update Sales Invoice  extended  by husnain 12-Dec-2023 to give priority to item sale account while making sale invoice
//
function extended_write_sales_invoice(&$invoice)
{
    global $Refs;

    $trans_no = $invoice->trans_no;
    if (is_array($trans_no))
        $trans_no = key($trans_no);

    $date_ = $invoice->document_date;
    $charge_shipping =$invoice->freight_cost;

    begin_transaction();

    hook_db_prewrite($invoice, ST_SALESINVOICE);
    $company_data = get_company_prefs();

    $branch_data = get_branch_accounts($invoice->Branch);

    $customer = get_customer($invoice->customer_id);

    add_new_exchange_rate($customer['curr_code'], $date_, $invoice->ex_rate);

    // offer price values without freight costs
    $items_total = $invoice->get_items_total_dispatch();
    $freight_tax = $invoice->get_shipping_tax();

    if (!$invoice->is_prepaid())
        update_customer_trans_version(get_parent_type(ST_SALESINVOICE), $invoice->src_docs);
    elseif (count($invoice->prepayments)) {	// partial invoice
        $last_payment = end($invoice->prepayments);
        $gl_date = sql2date($last_payment['tran_date']);
    } else {	// final invoice
        $gl_date = $invoice->document_date;
    }

    $ov_gst = 0;
    $taxes = $invoice->get_taxes(); // all taxes with freight_tax
    $dec = user_price_dec();
    foreach ($taxes as $taxitem) {
        $taxitem['Value'] =  round2($taxitem['Value'], $dec);
        $ov_gst +=  $taxitem['Value'];
    }

    if($invoice->tax_included==0) {
        $items_added_tax = $ov_gst-$freight_tax;
        $freight_added_tax = $freight_tax;
    } else {
        $items_added_tax = 0;
        $freight_added_tax = 0;
    }

    /* Insert/update the debtor_trans */
    $sales_order = $invoice->order_no;
    if (is_array($sales_order))
        $sales_order = $sales_order[0]; // assume all crucial SO data are same for every delivery

    if ($trans_no) {
        $allocs = get_payments_for($trans_no, ST_SALESINVOICE, $invoice->customer_id);
        delete_comments(ST_SALESINVOICE, $trans_no);
        void_gl_trans(ST_SALESINVOICE, $trans_no, true);
        void_trans_tax_details(ST_SALESINVOICE, $trans_no);
    } else
        $allocs = get_payments_for($invoice->order_no, ST_SALESORDER, $invoice->customer_id);

    if ($invoice->is_prepaid()) // selected prepayment is already in cart
    {
        $allocs = $invoice->prepayments;
        // values posted are reduced by prepaid_factor
        $prepaid_factor = $invoice->prep_amount/$invoice->get_trans_total();
    } else {
        $prepaid_factor = 1;
    }

    // write_customer_trans have to be called after optional void_cust_allocations above
    $invoice_no = write_customer_trans(ST_SALESINVOICE, $trans_no, $invoice->customer_id,
        $invoice->Branch, $date_, $invoice->reference, $items_total, 0,
        $items_added_tax, $invoice->freight_cost, $freight_added_tax,
        $invoice->sales_type, $sales_order, $invoice->ship_via,
        $invoice->due_date, 0, 0, $invoice->dimension_id,
        $invoice->dimension2_id, $invoice->payment, $invoice->tax_included, $invoice->prep_amount);

    if ($trans_no == 0) {
        $invoice->trans_no = array($invoice_no=>0);
    } else
        move_trans_attachments(ST_SALESINVOICE, $trans_no, $invoice_no);

    $total = 0;
    // for prepayments use deferred income account if set
    $sales_account = $invoice->is_prepaid() ? get_company_pref('deferred_income_act') : 0;

    foreach ($invoice->line_items as $line_no => $invoice_line) {
        $qty = $invoice_line->qty_dispatched;
        $line_taxfree_price = get_tax_free_price_for_item($invoice_line->stock_id,
            $invoice_line->price * $qty, 0, $invoice->tax_included,
            $invoice->tax_group_array);

        $line_tax = get_full_price_for_item($invoice_line->stock_id,
                $invoice_line->price * $qty, 0, $invoice->tax_included,
                $invoice->tax_group_array) - $line_taxfree_price;

        write_customer_trans_detail_item(ST_SALESINVOICE, $invoice_no, $invoice_line->stock_id,
            $invoice_line->item_description, $invoice_line->qty_dispatched,
            $invoice_line->line_price(), $qty ? $line_tax/$qty : 0, $invoice_line->discount_percent,
            $invoice_line->standard_cost, $invoice_line->src_id,
            $trans_no ? $invoice_line->id : 0);

        // Update delivery items for the quantity invoiced
        if ($invoice_line->qty_old != $invoice_line->qty_dispatched)
        {
            if ($invoice->is_prepaid())
                update_prepaid_so_line($invoice_line->src_id, $invoice_line->qty_dispatched-$invoice_line->qty_old);
            else
                update_parent_line(ST_SALESINVOICE, $invoice_line->src_id, ($invoice_line->qty_dispatched-$invoice_line->qty_old));
        }
        if ($invoice_line->qty_dispatched != 0) {
            $stock_gl_code = get_stock_gl_code($invoice_line->stock_id);

            if ($invoice_line->line_price() != 0) {
                //Post sales transaction to GL credit sales

                // If there is a Branch Sales Account, then override with this,
                // else take the Item Sales Account
                if (!$invoice->is_prepaid()) {
                    // by husnain 12-Dec-2023
                    //to check whether or not to give item sale account priority on the basis of the $use_item_sale_account property
                    if($invoice->use_item_sale_account) {    //If requested to use item sale account
                        $sales_account = ($stock_gl_code['sales_account'] != "" ? $stock_gl_code['sales_account']: $branch_data['sales_account']);
                    }
                    else{
                        $sales_account = ($branch_data['sales_account'] != "" ? $branch_data['sales_account'] : $stock_gl_code['sales_account']);
                    }
                }
                // If there is a Customer Dimension, then override with this,
                // else take the Item Dimension (if any)
                $dim = ($invoice->dimension_id != $customer['dimension_id'] ? $invoice->dimension_id :
                    ($customer['dimension_id'] != 0 ? $customer["dimension_id"] : $stock_gl_code["dimension_id"]));
                $dim2 = ($invoice->dimension2_id != $customer['dimension2_id'] ? $invoice->dimension2_id :
                    ($customer['dimension2_id'] != 0 ? $customer["dimension2_id"] : $stock_gl_code["dimension2_id"]));
                $total += add_gl_trans_customer(ST_SALESINVOICE, $invoice_no, $date_, $sales_account, $dim, $dim2,
                    -$line_taxfree_price*$prepaid_factor,
                    $invoice->customer_id, "The sales price GL posting could not be inserted");

                if ($invoice_line->discount_percent != 0) {

                    $total += add_gl_trans_customer(ST_SALESINVOICE, $invoice_no, $date_,
                        $branch_data["sales_discount_account"], $dim, $dim2,
                        ($line_taxfree_price * $invoice_line->discount_percent)*$prepaid_factor,
                        $invoice->customer_id, "The sales discount GL posting could not be inserted");
                } /*end of if discount !=0 */
            }
        } /*quantity dispatched is more than 0 */
    } /*end of delivery_line loop */

    if (($items_total + $charge_shipping) != 0) {
        $total += add_gl_trans_customer(ST_SALESINVOICE, $invoice_no, $date_, $branch_data["receivables_account"], $invoice->dimension_id, $invoice->dimension2_id,
            ($items_total + $charge_shipping + $items_added_tax + $freight_added_tax)*$prepaid_factor,
            $invoice->customer_id, "The total debtor GL posting could not be inserted");
    }
    $to_allocate = ($items_total + $charge_shipping + $items_added_tax + $freight_added_tax);

    if ($charge_shipping != 0) {
        $total += add_gl_trans_customer(ST_SALESINVOICE, $invoice_no, $date_, $company_data["freight_act"], $invoice->dimension_id, $invoice->dimension2_id,
            -$invoice->get_tax_free_shipping()*$prepaid_factor, $invoice->customer_id,
            "The freight GL posting could not be inserted");
    }
    // post all taxes
    foreach ($taxes as $taxitem) {
        if ($taxitem['Net'] != 0) {
            $ex_rate = get_exchange_rate_from_home_currency(get_customer_currency($invoice->customer_id), $date_);
            add_trans_tax_details(ST_SALESINVOICE, $invoice_no, $taxitem['tax_type_id'],
                $taxitem['rate'], $invoice->tax_included, $prepaid_factor*$taxitem['Value'],
                $taxitem['Net'], $ex_rate, $date_, $invoice->reference, TR_OUTPUT);
            if (isset($taxitem['sales_gl_code']) && !empty($taxitem['sales_gl_code']) && $taxitem['Value'] != 0)
                $total += add_gl_trans_customer(ST_SALESINVOICE, $invoice_no, $date_, $taxitem['sales_gl_code'], $invoice->dimension_id, $invoice->dimension2_id,
                    (-$taxitem['Value'])*$prepaid_factor, $invoice->customer_id,
                    "A tax GL posting could not be inserted");
        }
    }

    /*Post a balance post if $total != 0 */
    add_gl_balance(ST_SALESINVOICE, $invoice_no, $date_, -$total, PT_CUSTOMER, $invoice->customer_id);

    add_comments(ST_SALESINVOICE, $invoice_no, $date_, $invoice->Comments);

    if ($trans_no == 0) {
        $Refs->save(ST_SALESINVOICE, $invoice_no, $invoice->reference, null, $invoice->fixed_asset);
        if ($invoice->payment_terms['cash_sale'] && $invoice->pos['pos_account']) {
            $amount = $items_total + $items_added_tax + $invoice->freight_cost
                + $freight_added_tax;
            if ($amount != 0) {
                // to use debtors.pmt_discount on cash sale:
                // extend invoice entry page with final amount after discount
                // and change line below.
                $discount = 0; // $invoice->cash_discount*$amount;
                $payment_info = $invoice->pos['pos_name'].' #'.$invoice_no;
                if (!empty($invoice->payment_info))
                    $payment_info .= ' ' . $invoice->payment_info;
                $pmtno = write_customer_payment(0, $invoice->customer_id,
                    $invoice->Branch, $invoice->pos['pos_account'], $date_,
                    $Refs->get_next(ST_CUSTPAYMENT, null, array('customer' => $invoice->customer_id,
                        'branch' => $invoice->Branch, 'date' => $date_)),
                    $amount-$discount, $discount, $payment_info,
                    0,0,0,$invoice->dimension_id, $invoice->dimension2_id);
                add_cust_allocation($amount, ST_CUSTPAYMENT, $pmtno, ST_SALESINVOICE, $invoice_no, $invoice->customer_id, $date_);

                update_debtor_trans_allocation(ST_SALESINVOICE, $invoice_no, $invoice->customer_id);
                update_debtor_trans_allocation(ST_CUSTPAYMENT, $pmtno, $invoice->customer_id);
            }
        }
    }
    reallocate_payments($invoice_no, ST_SALESINVOICE, $date_, $to_allocate, $allocs, $invoice->customer_id);
    hook_db_postwrite($invoice, ST_SALESINVOICE);

    commit_transaction();

    return $invoice_no;
}

//--------------------------------------------------------------------------------------------------