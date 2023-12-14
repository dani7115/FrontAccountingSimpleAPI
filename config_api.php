<?php

if (file_exists(__DIR__ . '/_frontaccounting')) {
	$rootPath = realpath(__DIR__ . '/_frontaccounting');
} else {
	$rootPath = realpath(__DIR__ . '/../..');
}

define('API_ROOT', $rootPath . '/modules/FrontAccountingSimpleAPI');    //change to api if your api module directory name is api
define('FA_ROOT', $rootPath);

