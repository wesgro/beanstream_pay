<?php

/**
 * @file
 */

class pay_method_gateway_beanstream_pay extends pay_method_gateway {
  var $beanstream_pay_api_id = '';
  var $beanstream_pay_password = '';
  var $beanstream_pay_email_customer = 0;
  var $beanstream_pay_email_merchant = 1;
  var $beanstream_pay_developer = 0;

  var $gateway_supports_ach = TRUE;
  var $gateway_supports_cc = TRUE;
  var $gateway_supports_recurring = TRUE;

  var $beanstream_pay_payment_type = 'payment'; // Payment/Periodic

  // this variable could be replaced when currency support in included in pay
  // http://drupal.org/node/932666
  var $currency = 'CAD';

  function gateway_url() {
    $url = 'https://api.Beanstream/xmlapi/'. $this->beanstream_pay_payment_type;
    if ($this->beanstream_pay_developer) {
      $url = 'https://test.Beanstream/xmlapi/'. $this->beanstream_pay_payment_type;
    }
    return $url;
  }

  /**
   * Create request to be POST to the gateway.
   */
  function gateway_request() {
    // create a unique id for the message
    $message_id    = md5($this->beanstream_pay_api_id . time());
    $timeout       = 60;
    $timestamp     = date('YmdHis000+600');

    $api_version   = 'xml-4.2';
    if ($this->beanstream_pay_payment_type != 'payment') {
      $api_version   = 'spxml-3.0'; // Periodic
    }
    $merchant_info = array(
      'MessageInfo' => array(
        'messageID' => $message_id,
        'messageTimestamp' => $timestamp,
        'timeoutValue' => $timeout,
        'apiVersion' => $api_version
      ),
      'MerchantInfo' => array(
        'merchantID' => $this->beanstream_pay_api_id,
        'password' => $this->beanstream_pay_password,
      ),
      'RequestType' => ($this->beanstream_pay_payment_type == 'payment') ? 'Payment' : 'Periodic',
    );
   
    $data = array_merge($merchant_info, $this->gateway_request_values());

    // Set the transaction type based on requested activity.
    if (!$txntype = $this->beanstream_pay_trxtype($this->activity->activity)) {
      drupal_set_message("Payment activity '$this->activity->activity' is unsupported. Not processing transaction", 'error');
      return FALSE;
    }

    $xml = $this->arrayToXml($data, 'SecurePayMessage');
    return $xml;
  }
 
  /**
   *
   */
  function gateway_response($result) {
    $xml = simplexml_load_string($result->data);

    dsm($xml);

    // error connecting/communicating to gateway.
    if ($xml->Status->statusCode != '000') {
      watchdog('payment', "Error processing payment: Beanstream gateway returned '@err'", array('@err' => (string)$xml->Status->statusDescription));
      drupal_set_message("Error processing payment, contact site administration.");
      return FALSE;
    }

    $transaction = $xml->Payment->TxnList->Txn;

    // Save the transaction ID for tracking and/or future operations.
    $this->activity->identifier = (string)$transaction->txnID;

    if ((string)$transaction->approved != 'Yes') {
      watchdog('payment', "Error processing payment: Beanstream gateway returned '@err'", array('@err' => (string)$transaction->responseText));
      drupal_set_message("Error processing payment, please check your card details and try again.");
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Process a payment.
   */
  function execute($activity) {
    $this->activity = $activity;
    if ($request = $this->gateway_request()) {
      $response = drupal_http_request($this->gateway_url(), $this->gateway_headers(), 'POST', $request);
      if ($response->error) {
        watchdog('payment', "Gateway Error: @err Payment NOT processed.", array('@err' => $ret->error));
        $this->activity->data = (array) $ret;
        return FALSE;
      }
      else {
        return $this->gateway_response($response);
      }
    }
  }

  /**
   * @return
   *   Array of HTTP headers in key => value pairs.
   */
  function gateway_headers() {
    return array('Content-Type' => 'text/xml');
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
    // S:Sale C:Credit A:Auth D:Delayed V:Void F:Voice I:Inquiry N:Duplicate    
    $trxtypes = array(
      'complete'  => 'AUTH_CAPTURE',
      //'authorize' => 'AUTH_ONLY',
      //'capture'   => 'CAPTURE_ONLY',
      //'void'      => 'VOID',
      //'credit'    => 'CREDIT',
    );
    return $trxtypes[$activity];
  }

  // TODO
  function gateway_request_values() {
    $data = array(
      'Payment' => array(
        'TxnList' => array(
          '_a' => array('count' => '1'),
          '_c' => array(
            'Txn' => array(
              '_a' => array('ID' => '1'), 
              '_c' => array(
                'txnType' => '0',  // 0 = Standard Payment
                'txnSource' => '23',  // 23 = XML
                'amount' => (int)($this->total()*100), // amount takes a value in cents
                'currency' => $this->currency,
                'purchaseOrderNo' => 'test',
                'CreditCardInfo' => array(
                  'cardNumber' => $this->cc_number,
                  'expiryDate' => $this->cc_exp_month .'/'. $this->cc_exp_year,
                  'ccv'        => $this->cc_ccv2,
                ),
              ),
            ),
          ),
        ),
      ),
    );

    return $data;
  }

  /**
   * The main function for converting to an XML document.
   * Pass in a multi dimensional array and this recrusively loops through and builds up an XML document.
   *
   * @param array $data
   * @param string $rootNodeName - what you want the root node to be - defaultsto data.
   * @param SimpleXMLElement $xml - should only be used recursively
   * @return string XML
   */
  function arrayToXml($data, $rootNodeName = 'data', $xml=null) {
    if ($xml == null) {
      $xml = simplexml_load_string("<?xml version='1.0' encoding='utf-8'?><$rootNodeName />");
    }
     
    foreach($data as $key => $value) {
      if (is_numeric($key)) {
        // make string key...
        $key = "unknownNode_". (string) $key;
      }

      // replace anything not alpha numeric
      $element = preg_replace('/[^a-z]/i', '', $key);
         
      // if there is another array found recrusively call this function
      if (is_array($value)) {
        $node = $xml->addChild($element);
        if (isset($value['_c'])) { 
          $this->arrayToXml($value['_c'], $rootNodeName, $node);
        }
        else {
          // recrusive call.
          $this->arrayToXml($value, $rootNodeName, $node);
        }
        if (isset($value['_a'])) {
          foreach ($value['_a'] as $at => $av) {
            $node->addAttribute($at, $av);
          }
        }
      }
      else {
        // add single node.
        $value = htmlentities($value);
        $xml->addChild($key,$value);
      }
    }
    // pass back as string. or simple xml object if you want!
    return $xml->asXML();
  }

}
