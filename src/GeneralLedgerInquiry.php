<?php
/**
 * Author : Danish Naseem <dani.115115@gmail.com>
 */

namespace FAAPI;

class GeneralLedgerInquiry
{
    public function post($rest)
    {
        include_once(API_ROOT . "/generalLedgerInquiry.inc");
        inquiry();
    }

}
