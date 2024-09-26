<?php
/**
 * Author : Danish Naseem <dani.115115@gmail.com>
 */

namespace FAAPI;



class CustomerPayments
{
    public function post($rest)
    {
        include_once(API_ROOT . "/customerPayments.inc");
        customerPayments_add();
    }

    public function delete($rest)
    {
        include_once(API_ROOT . "/customerPayments.inc");
        customerPayments_delete();
    }

    public function getAvailableCustomerPayments($rest)
    {
        include_once(API_ROOT . "/customerPayments.inc");
        getAvailableCustomerPayments();
    }

    public function getCustomerSalesForAllocate($rest)
    {
        include_once(API_ROOT . "/customerPayments.inc");
        getCustomerSalesForAllocate();
    }

    public function allocateCustomerPayments($rest)
    {
        include_once(API_ROOT . "/customerPayments.inc");
        customerPayments_allocate();
    }
}
