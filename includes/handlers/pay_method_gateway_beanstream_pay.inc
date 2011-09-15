<?php
/**
 * @file
 */


class pay_method_gateway_beanstream_pay extends pay_method_gateway {
	var $beanstream_pay_api_id = '';
	var $beanstream_pay_test_id = '';
	var $beanstream_pay_password = '';
	var $beanstream_pay_user = '';
	var $beanstream_pay_email_customer = 0;
	var $beanstream_pay_email_merchant = 1;
	var $beanstream_pay_developer = 0;

	var $gateway_supports_ach = false;
	var $gateway_supports_cc = TRUE;
	var $gateway_supports_recurring = false;

	/**
	 * A list of all currencies supported by beanstream_pay.
	 */
	function available_currencies() {
		return beanstream_pay_currencies();
	}

	function payment_types($filter = NULL) {
		$payment_types = parent::payment_types($filter);


		if ($filter) {
			foreach ($payment_types as $key => $label) {
				if (!$filter[$key]) unset($payment_types[$key]);
			}
		}

		return $payment_types;
	}

	function gateway_url() {
		$url = 'https://www.beanstream.com/scripts/process_transaction.asp';
		return $url;
	}

	function gateway_request() {

		// watchdog('payment', "Activity returned was '@err'", array('@err' => print_r($this->beanstream_pay_trxtype($this->activity->activity), true)));
		// Set the transaction type based on requested activity.
		if (!$txntype = $this->beanstream_pay_trxtype($this->activity->action)) {
			drupal_set_message("Payment activity '$this->activity->activity' is unsupported. Not processing transaction", 'error');
			return false;
		}

		$req = '';
		foreach ($this->gateway_request_values($txntype) as $key => $val) {
			if ($val = trim($val)) $req .= check_plain($key) .'='. urlencode($val) .'&';
		}
		//krumo($req); exit;

		return $req;
	}

	function execute() {
		if ($request = $this->gateway_request()) {
			$ch = curl_init();
			curl_setopt($ch, CURLOPT_URL,$this->gateway_url());
			curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
			curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 0);
			curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, 0);
			curl_setopt($ch, CURLOPT_POST, 1);
			curl_setopt( $ch, CURLOPT_POSTFIELDS, $this->gateway_request() );

			$response = curl_exec($ch);

			if (curl_getinfo($ch, CURLINFO_HTTP_CODE) != 200) {
				watchdog('payment', "Gateway Error: @err Payment NOT processed. Response was @resp", array('@err' => curl_getinfo($ch, CURLINFO_HTTP_CODE), '@resp' =>print_r($this->http_query_to_array($response),true)));
				$ret = $this->http_query_to_array($response);
				$this->activity->data = (array) $ret;
				$this->activity->result = FALSE;
			}
			else {
				$this->activity->result = $this->gateway_response($response);
			}
			//close the curl handle
			curl_close($ch);
			// Return TRUE or FALSE on success/failure.
			// krumo( ($this->activity->result == true));

			return ($this->activity->result == true);
		}
	}

	function gateway_response($result = NULL) {
		//krumo($result);


		$r = $this->http_query_to_array($result);
		// Save the transaction ID for tracking and/or future operations.
		$this->activity->identifier = $r['trnId'];
		

		$response_code = $r['trnApproved'];

		$this->activity->data = array(
			'response_string' => $r['messageText'],
			'approval_code'   => $r['authCode'],
			'transaction_total' => $r['trnAmount'],
		);
		if ($response_code <> '1') {
			watchdog('payment', "Error processing payment: beanstream_pay gateway returned '@err'", array('@err' => $this->activity->data['response_string']));
			drupal_set_message("Error processing payment: beanstream_pay gateway returned '". $this->activity->data['response_string']. "'", 'error');
		}

		return ($response_code == '1');
	}

	function settings_form(&$form, &$form_state) {
		parent::settings_form($form, $form_state);
		$group = $this->handler();

		$form[$group]['an']['#type'] = 'fieldset';
		$form[$group]['an']['#collapsible'] = FALSE;
		$form[$group]['an']['#title'] = t('Beanstream settings');
		$form[$group]['an']['#group'] = $group;

		$form[$group]['an']['beanstream_pay_api_id'] = array(
			'#type' => 'textfield',
			'#title' => t('Beanstream API Merchant ID'),
			'#default_value' => $this->beanstream_pay_api_id,
			'#required' => TRUE,
			'#parents' => array($group, 'beanstream_pay_api_id'),
		);
		$form[$group]['an']['beanstream_pay_test_id'] = array(
			'#type' => 'textfield',
			'#title' => t('Beanstream Test ID'),
			'#default_value' => $this->beanstream_pay_test_id,
			'#required' => TRUE,
			'#parents' => array($group, 'beanstream_pay_test_id'),
		);
		$form[$group]['an']['beanstream_pay_user'] = array(
			'#type' => 'textfield',
			'#title' => t('Beanstream User'),
			'#default_value' => $this->beanstream_pay_user,
			'#required' => TRUE,
			'#parents' => array($group, 'beanstream_pay_user'),
		);
		$form[$group]['an']['beanstream_pay_password'] = array(
			'#type' => 'textfield',
			'#title' => t('Beanstream Password'),
			'#default_value' => $this->beanstream_pay_password,
			'#required' => TRUE,
			'#parents' => array($group, 'beanstream_pay_password'),
		);
		$form[$group]['an']['beanstream_pay_developer'] = array(
			'#type' => 'checkbox',
			'#title' => t('This is a developer test account'),
			'#description' => t('If the above values belong to a developer test account and not a live merchant account, check this box.'),
			'#default_value' => $this->beanstream_pay_developer,
			'#parents' => array($group, 'beanstream_pay_developer'),
		);
		$form[$group]['an']['beanstream_pay_email_customer'] = array(
			'#type' => 'checkbox',
			'#title' => t('E-mail a reciept to the customer'),
			'#default_value' => $this->beanstream_pay_email_customer,
			'#parents' => array($group, 'beanstream_pay_email_customer'),
		);
		$form[$group]['an']['beanstream_pay_email_merchant'] = array(
			'#type' => 'checkbox',
			'#title' => t('E-mail a reciept to the merchant'),
			'#default_value' => $this->beanstream_pay_email_merchant,
			'#parents' => array($group, 'beanstream_pay_email_merchant'),
		);
	}


	function beanstream_pay_trxtype($activity) {
		$trxtypes = array(
			'complete'  => 'P',
			'authorize' => 'PA',
		);
		return $trxtypes[$activity];
	}

	// TODO
	function gateway_request_values($txntype) {
		$data = array(
			'requestType'           => 'BACKEND',
			'trnType'               => 'P',
			'trnComments'           => $this->billto['notes'],
			'username'              => $this->beanstream_pay_user,
			'password'              => $this->beanstream_pay_password,
			'merchant_id'           => $this->beanstream_pay_api_id,
			'trnAmount'             => $this->total(),
			'trnCardNumber'         => $this->cc_number,
			'trnExpMonth'           => $this->cc_exp_month,
			'trnExpYear'            => $this->cc_exp_year,
			'trnCardCvd'            => $this->cc_ccv2,
			'ordName'               => $this->first_name .' '.$this->last_name,
			'trnCardOwner'          => $this->first_name .' '.$this->last_name,
			'ordAddress1'           => $this->billto['street1'],
			'ordCity'               => $this->billto['city'],
			'ordProvince'           => $this->billto['state'],
			'ordPostalCode'         => $this->billto['zip'],
			'ordCountry'            => 'ca',
			'ordPhoneNumber'        => $this->billto['phone'],
			'ordEmailAddress'       => $this->mail,
		);


		if ($this->gateway_testmode) {
			$data['trnCardNumber']       = '5100000010001004';
			$data['trnCardCvd']          = '123';
			$data['merchant_id']         = $this->beanstream_pay_test_id;
			$data['ordAddress1']         = '123 test st';
			$data['ordCity']             = 'victoria';
			$data['ordProvince']         = 'bc';
			$data['ordPostalCode']       = 'v5v4g4';
			$data['ordCountry']          = 'ca';
			$data['ordPhoneNumber']      = '555-555-5555';
		}

		return $data;
	}

	function http_query_to_array($query){
		$result = urldecode($query);
		$response_string = explode('&', ($result));
		$response = array();
		foreach($response_string as $key => $param) {
			list($key, $value) = explode('=', $param);
			$r[$key] = $value;
		}
		return $r;
	}

}
