<?php

/**
 * @file
 * Admin page callback file for the sync db module.
 */

// <editor-fold defaultstate="collapsed" desc="Module Config Form">

function sync_db_admin_page() {
  $form['sync_db_attempts_amt_sync'] = array(
    '#type' => 'textfield',
    '#title' => t('Amount of attempts'),
    '#default_value' => variable_get('sync_db_attempts_amt_sync', 3),
    '#size' => 2,
    '#maxlength' => 2,
    '#description' => t('The amount of attempts that the module should try sync to consider it a failure'),
    '#required' => TRUE
  );
  
  $form['sync_db_interval_sync'] = array(
    '#type' => 'textfield',
    '#title' => 'Interval',
    '#default_value' => variable_get('sync_db_interval_sync', 5),
    '#size' => 4,
    '#maxlength' => 3,
    '#description' => t('Time to wait in minutes between each sync attempt'),
    '#required' => TRUE
  );
  
  return system_settings_form($form);
}

function sync_db_admin_page_validate($form, &$form_state) {
  $attempts = $form_state['values']['sync_db_attempts_amt_sync'];
  $min = 1;
  $max = 5;
  if (!is_numeric($attempts) || $attempts < $min || $attempts > $max) {
    form_set_error('sync_db_attempts_amt_sync', t('The amount of attempts must be an integer between !min and !max', array(
      '!min' => $min,
      '!max' => $max
      )
    ));
  }
  
  $minutes = $form_state['values']['sync_db_interval_sync'];
  $min = 1;
  $max = 60;
  if (!is_numeric($minutes) || $minutes < $min || $minutes > $max) {
    form_set_error('sync_db_interval_sync', t('The interval must be a value between !min and !max minutes', array(
      '!min' => $min,
      '!max' => $max
      )
    ));
  }
}

// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="Synchronization">

/**
 * Checks if the connection is valid to sync, 
 * if so redirects to sync_db_synchronize_connection
 * 
 * @param ConnectionInfo $connection
 * @see sync_db_synchronize_connection
 */
function sync_db_synchronize_connection_page($connection) {
  if ($connection instanceof ConnectionInfo) {
    return
      drupal_get_form('sync_db_synchronize_connection', $connection);
  }
  
  return MENU_NOT_FOUND;
}

/**
 * Synchronization form for the connection. 
 * Page callback from sync_db_synchronize_connection_page
 * 
 * @param ConnectionInfo $con
 * @see sync_db_synchronize_connection_page
 */
function sync_db_synchronize_connection($form, &$form_state, $con) {
  if (!isset($form_state['storage']['track_id'])) {
    $form_state['storage']['track_id'] = REQUEST_TIME;
  }
  $form_state['storage']['connection'] = $con;
  
  $form['#prefix'] = '<div id="sync-db-form-wrapper">';
  $form['#suffix'] = '</div>';
  $form['#attached']['css'] = array(
    drupal_get_path('module', 'sync_db') . '/sync_db.css'
  );
  $form['#attached']['js'] = array(
    drupal_get_path('module', 'sync_db') . '/sync_db.js'
  );
  
  $show_report = FALSE;
  if (isset($form_state['storage']['sync'])) {
    if ($form_state['storage']['sync']->getReport()) {
      $show_report = TRUE;
    }
  }  
  
  if (!$show_report) {
    $form['options'] = _sync_db_build_synchronize_connection_options();
    
    $form['partial'] = array(
      '#type' => 'checkbox',
      '#title' => t('Partial synchronization'),
      '#ajax' => array(
        'wrapper' => 'sync-db-tables-wrapper',
        'callback' => 'sync_db_synchronize_connection_partial_callback'
      )
    );
    
    $form['change_partial'] = array(
      '#type' => 'submit',
      '#value' => t('Change'),
      '#attributes' => array(
          'class' => array('no-ajax')),
      '#limit_validation_errors' => array(),
      '#validate' => array()
    );

    $is_partial = FALSE;
    if (isset($form_state['values']['partial'])) {
      $is_partial = $form_state['values']['partial'];
    }
    elseif (isset($form_state['input']['partial'])) {
      $is_partial = TRUE;
    }

    $form['tables_to_sync'] = array(
      '#title' => t('Tables to be sync'),
      '#prefix' => '<div id="sync-db-tables-wrapper">',
      '#suffix' => '</div>',
      '#header' => array(
        'name' => t('Table'),     
        'localname' => t('Local name'),
        'condition' => t('Condition'),
      ),
    );

    if ($is_partial) {
      $form['tables_to_sync']['#type'] = 'tableselect';

      $opts = array();
      foreach($con->getTables() as $name => $tab) {
        $opts[$name] = array(
          'name' => $name,
          'localname' => $tab['local_name'],
          'condition' => $tab['condition']
        );
      }
      $form['tables_to_sync']['#options'] = $opts;
    }
    else {
      $form['tables_to_sync']['#theme'] = 'table';

      $rows = array();
      foreach($con->getTables() as $tab) {
        $rows[] = array(
          $tab['real_name'],
          $tab['local_name'],
          $tab['condition']);
      }
      $form['tables_to_sync']['#rows'] = $rows;
    }
    
    $form['warning'] = array(
      '#type' => 'markup',
      '#prefix' => '<div>',
      '#suffix' => '</div>',
      '#markup' => '<div class="messages warning no-ajax">'. t("When ajax is disabled you can't track the progress.") .'</div>'
    );
    
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => 'Sync',
      '#id' => 'sync-db-submit',
      '#ajax' => array(
        'callback' => 'sync_db_synchronize_connection_callback',
        'wrapper' => 'sync-db-form-wrapper',
        'progress' => array(
          'type' => 'bar',
          'url' => url('sync-db/synchronization/progress/' . $form_state['storage']['track_id']),
          'interval' => 1000//1s
        )
      )
    );
  }
  else { //There's a report to show
    drupal_set_title(t('Synchronization report'));    
    $form += _sync_db_build_synchronize_connection_report($form, $form_state);
  }
  
  return $form;
}

/**
 * Callback function for building the sync options form array
 * 
 * @return The options form array
 */
function _sync_db_build_synchronize_connection_options() {
  $options = array(
    '#type' => 'fieldset',
    '#title' => t('Options'),
    '#collapsible' => TRUE
  );
  
  $options['verify'] = array(
    '#type' => 'checkbox',
    '#title' => t('Check structure (Recommended)'),
    '#default_value' => TRUE,
    '#id' => 'sync-db-verify'
  );
  
  $options['autorepair'] = array(
    '#type' => 'checkbox',
    '#description' => t('If any problem is found, repair the structure without warnings'),
    '#title' => t('Auto-Repair'),
    '#id' => 'sync-db-auto'
  );
  
  $options['fullsync'] = array(
    '#type' => 'checkbox',
    '#description' => t('Ignore conditions when retrieving third-party data (It may take longer)'),
    '#title' => t('Full')
  );
  
  return $options;
}

/**
 * Callback function for building the report form display
 * @see sync_db_synchronize_connection
 * 
 * @return The report form array 
 */
function _sync_db_build_synchronize_connection_report($form, &$form_state) {
  /* @var $report Synchronization */
  $sync = $form_state['storage']['sync'];
  $report = $sync->getReport();

  // <editor-fold defaultstate="collapsed" desc="General report">

  $form['general'] = array(
    '#type' => 'fieldset',
    '#title' => t('General'),
    '#collapsible' => TRUE
  );

  $form['general']['total'] = array(
    '#type' => 'item',
    '#title' => t('Total records found'),
    '#markup' => $report['status']['records']
  );

  $form['general']['inserted'] = array(
    '#type' => 'item',
    '#title' => t('Total records inserted'),
    '#markup' => $report['status']['inserted']
  );

  $form['general']['updated'] = array(
    '#type' => 'item',
    '#title' => t('Total records updated'),
    '#markup' => $report['status']['updated']
  );

  $form['general']['fails'] = array(
    '#type' => 'item',
    '#title' => t('Total records failed'),
    '#markup' => $report['status']['fails']
  );

  // </editor-fold>

  //Individual table report
  foreach ($report['tables'] as $table => $tabreport) {
    $form['tables'][$table] = array(
      '#type' => 'fieldset',
      '#title' => $table,
      '#collapsible' => TRUE,
      '#collapsed' => TRUE
    );

      $form['tables'][$table]['total'] = array(
        '#type' => 'item',
        '#title' => t('Records found'),
        '#markup' => $tabreport['records']
      );

      $form['tables'][$table]['inserted'] = array(
        '#type' => 'item',
        '#title' => t('Records inserted'),
        '#markup' => $tabreport['inserted']
      );

      $form['tables'][$table]['updated'] = array(
        '#type' => 'item',
        '#title' => t('Records updated'),
        '#markup' => $tabreport['updated']
      );

      if (isset($tabreport['failed'])) {
        $form['tables'][$table]['fails'] = array(
          '#type' => 'fieldset',
          '#title' => t('FAILED RECORDS') . ' - (' . count($tabreport['failed']) . ')',
          '#collapsible' => TRUE,
          '#collapsed' => TRUE
        );

        $form['tables'][$table]['fails']['data'] = array(
          '#theme' => 'table',
          '#header' => array_keys($tabreport['failed'][0]),
          '#rows' => $tabreport['failed'],            
        );
      }
  }

  // Sync failed rows option
  if ($report['status']['fails'] > 0) {
    $form['options'] = _sync_db_build_synchronize_connection_options();

    $form['options']['sync'] = array(
      '#type' => 'submit',
      '#value' => t('Sync failed records'),
      '#id' => 'sync-db-submit'
      //'#validate' => array('sync_db_synchronize_connection_validate');
    );
  }

  $form['ok'] = array(
    '#type' => 'submit',
    '#value' => t("That's okay"),
  );

  return $form;
}

/**
 * Callback function for displaying table when partial sync changes
 * @see sync_db_synchronize_connection
 */
function sync_db_synchronize_connection_partial_callback($form, &$form_state) {
  return $form['tables_to_sync'];
}

/**
 * Callback function for sync_db_synchronize_connection
 */
function sync_db_synchronize_connection_callback($form, &$form_state) {
  return $form;
}

/**
 * Callback function to validate sync_db_synchronize_connection form 
 * @see sync_db_synchronize_connection
 */
function sync_db_synchronize_connection_validate($form, &$form_state) {
  if (isset($form_state['values']['partial']))
  {
    if ($form_state['values']['partial']) {
      $selected = array_filter($form_state['values']['tables_to_sync']);
      if (empty($selected)) {
        form_set_error('tables_to_sync', t('Select at least one table to sync'));
      }
    }
  }
}

/**
 * Submit callback for sync_db_synchronize_connection
 * @see sync_db_synchronize_connection
 */
function sync_db_synchronize_connection_submit($form, &$form_state) {  
  
  //$form_state['storage']['values']['partial'] = $form_state['values']['partial'];  
  if ($form_state['triggering_element']['#id'] == t('sync-db-submit')) {
   
    $isSyncCreated = isset($form_state['storage']['sync']);
    $hasReport = FALSE;
    if ($isSyncCreated) {
      if ($form_state['storage']['sync']->getReport())
        $hasReport = TRUE;
    }
    
    if (!$isSyncCreated || !$hasReport) {
      $form_state['storage']['tables'] = ($form_state['values']['partial']) ?
          array_filter($form_state['values']['tables_to_sync']) : array_keys($form_state['storage']['connection']->getTables());
    }
    
    sync_db_synchronize_connection_perform_sync($form, $form_state);
    $form_state['rebuild'] = TRUE;
  }
  else {
    if ($form_state['triggering_element']['#value'] == t('Change')) {
      $form_state['rebuild'] = TRUE;      
    }
    else {
      $form_state['rebuild'] = FALSE;
      $form_state['redirect'] = sync_db_connection_path($form_state['storage']['connection'], 'tables');
    }
  }
}

/**
 * Function for perfoming synchronization and storing its instance in form_state
 * @see sync_db_synchronize_connection_submit
 */
function sync_db_synchronize_connection_perform_sync($form, &$form_state) {
  
  $mes = '';
  $mes_type = 'status';
  
  try {
    $isSyncCreated = isset($form_state['storage']['sync']);
    $hasReport = FALSE;
    if ($isSyncCreated) {
      if ($form_state['storage']['sync']->getReport())
        $hasReport = TRUE;
    }
    
    if (!$isSyncCreated || !$hasReport) {
      /* @var $con ConnectionInfo */
      $con = $form_state['storage']['connection'];
      $tables = $form_state['storage']['tables'];

      $sync = Synchronization::createFromConnection($con);

      //Options ➡Des.013
      $options['track'] = $form_state['storage']['track_id'];
      if ($form_state['values']['verify']) {
        $options['verify'] = $form_state['values']['autorepair'] ? 2 : 1;
      }
      $options['type'] = $form_state['values']['fullsync'];

      $sync->synchronize($tables, $options);
      $form_state['storage']['sync'] = $sync;
    }
    else { //Just failed rows
      $sync = $form_state['storage']['sync'];
      $sync->synchronizeFailedRows();
    }

    $report = $sync->getReport();

    if ($report['status']['inserted'] > 0) {
      $mes .= t('!inserted record(s) inserted.', array('!inserted' => $report['status']['inserted'])) . ' ';
    }
    
    if ($report['status']['updated'] > 0) {
      $mes .= t('!updated record(s) updated.', array('!updated' => $report['status']['updated'])) . ' ';
    }
    
    if (!$mes) {
      $mes = t('No changes.') . ' ';
    }
    
    if ($report['status']['records'] > 0) {
      $mes .= t('!total record(s) found.', array('!total' => $report['status']['records']));
    }
  }
  catch (StructureGapsException $ex) {
    $mes = t("There are !qtd table(s) to be repaired. "
            . "You can either check for !link, check 'Auto-Repair' option, or just uncheck 'Check structure' option (not recommended).", array(
              '!qtd' => $ex->getAmountGaps(),
              '!link' => l('details here',sync_db_connection_path($form_state['storage']['connection'], 'repair')
            )));
    $mes_type = 'error';
  } 
  catch (SyncDB_Exception $ex) {
    $mes_type = 'error';
    $mes = t('Something gone wrong: ') . $ex->getMessage();
  }
  finally {
    drupal_set_message($mes, $mes_type);
  }
}

/**
 * Outputs progress and message as json
 * 
 * @param int $track_id The ID to track the progress
 */
function sync_db_synchronize_connection_progress($track_id) {
  drupal_json_output(Synchronization::getProgress($track_id));
}

// </editor-fold>