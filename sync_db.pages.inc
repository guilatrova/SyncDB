<?php

/**
 * Builds and returns the connection form array based on form_id and step.
 * Used by add and edit connection forms.
 * @see sync_db_add_connection
 * @see sync_db_edit_connecton
 * 
 * @return A renderable form array 
 */
function sync_db_build_connection_form(&$form_state) {
  $step = empty($form_state['storage']['step']) ? 1 : $form_state['storage']['step'];
  $form_state['storage']['step'] = $step;
  
  $form['#prefix'] = '<div id="syncdb-wizard-form-wrapper">';
  $form['#suffix'] = '</div>';
  $form['#tree'] = TRUE;
  
  $form_id = $form_state['build_info']['form_id'];
  
  //Case 1 and 2 is generic, but 3 is exclusive of the edit function
  switch ($step) {
    case 1:
    $form['conname'] = array(
      '#type' => 'textfield',
      '#title' => t('Connection Name'),
      '#description' => t('An identifier for this connection'),
      '#required' => TRUE,
      '#maxlength' => 20,
    );

    $form['dbname'] = array(
      '#type' => 'textfield',
      '#title' => t('Database Name'),
      '#required' => TRUE,
      '#maxlength' => 50,
    );

    $form['host'] = array(
      '#type' => 'textfield',
      '#title' => t('Host'),
      '#required' => TRUE,
      '#maxlength' => 45,
    );

    $form['username'] = array(
      '#type' => 'textfield',
      '#title' => t('Username'),
      '#description' => t('The user which will be used to login to this database'),
      '#maxlength' => 45,
    );

    $form['password'] = array(
      '#type' => 'password',
      '#title' => t('Password'),
      '#description' => t('The password login for this database'),
      '#maxlength' => 45,
    );

    $form['driver'] = array();

    $form['description'] = array(
      '#type' => 'textarea',
      '#title' => t('Description'),
      '#maxlength' => 500
    );
    
    $form['next'] = array(
      '#type' => 'submit',
      '#value' => t('Next'),
      '#validate' => array('sync_db_connection_name_validate', 'sync_db_try_connect_validate'),
      '#ajax' => array(
        'callback' => 'sync_db_connection_wizard_form_callback',
        'wrapper' => 'syncdb-wizard-form-wrapper'
      )
    );

    break;

    case 2:
      $header = array(
        'name' => t('Table'), 
        'description' => t('Description'),
        'localname' => t('Local name'),
        'condition' => t('Condition')
      );

      $prevstep = $form_state['storage']['values']['step1'];
      if ($form_id == 'sync_db_edit_connection') {
        $driver = $form_state['storage']['connection']->getDriver();
      }
      else {
        $driver = $prevstep['driver'];
      }
      $tables = ConnectionTableGetter::getTables(
          $prevstep['dbname'],
          $prevstep['host'],
          $prevstep['username'],
          $prevstep['password'],
          $driver);    
    
      $options = array();
      foreach($tables as $name => $descript) {
        $options[$name] = array(
          'name' => $name,
          'description' => $descript,
          'localname' => array(
            'data' => array(
              '#type' => 'textfield',
              '#maxlength' => 50,
              '#name' => "localname[$name]",
              '#attributes' => array(
                'placeholder' => t("'!tabname' will be assumed", array('!tabname' => $name))
              )
            )
          ),
          'condition' => array(
            'data' => array(
              '#type' => 'textfield',
              '#maxlength' => 50,
              '#name' => "condition[$name]",
            )
          )
        );
      }

      $form['localname'] = array(
        '#type' => 'value'
      );

      $form['condition'] = array(
        '#type' => 'value'
      );

      $form['help'] = array(
        '#type' => 'item',
        '#title' => t('Select the tables which you want to synchronize.'),
        '#description' => '<p>'. t('If you don\'t want to rename a table that you\'ve selected, '
            . 'just let the field "Local name" blank and the original name will be assumed.') . '</p>',
      );

      $form['tables'] = array(
        '#type' => 'tableselect',
        '#header' => $header,
        '#options' => $options,
      );

      $form['next'] = array(
        '#type' => 'submit',
        '#value' => t('Save'),
        '#validate' => array('sync_db_selected_tables_validate'),
      );

      break;          
  }
  
  if ($step > 1) { //It need a previous button?
    $form['prev'] = array(
      '#type' => 'submit',
      '#value' => t("Previous"),

      // Since all info will be discarded, don't validate on 'prev'.
      '#limit_validation_errors' => array(),
      // #submit is required to use #limit_validation_errors
      '#submit' => array('sync_db_add_connection_submit'),
      '#ajax' => array(
        'wrapper' => 'syncdb-wizard-form-wrapper',
        'callback' => 'sync_db_connection_wizard_form_callback',
      ),
    );
  }
  
  return $form;
}

/**
 * The callback function used by the wizard forms for rendering
 */
function sync_db_connection_wizard_form_callback($form, &$form_state) {
  return $form;
}

// <editor-fold defaultstate="collapsed" desc="Add connection">

/**
 * Builds the form for setting up a new connection
 */
function sync_db_add_connection($form, &$form_state) {
  $form = sync_db_build_connection_form($form_state);
  
  //If the user return to a previous step, we will retrieve the values
  if ($form_state['storage']['step'] == 1) {
    $form['driver'] = array(
      '#type' => 'select',
      '#title' => t('Driver'),
      '#default_value' => '0',
      '#options' => sync_db_available_drivers(),
      '#description' => t('The database engine'),
      '#required' => TRUE,
    );
    
    foreach($form as $key => &$field) {
      if (!empty($form_state['storage']['values']['step1'][$key]))
        $field['#default_value'] = $form_state['storage']['values']['step1'][$key];
    }
  }
  
  return $form;
}

/**
 * Submit callback from sync_db_add_connection, while the step doesnt 
 * achieve the last it will render again
 * 
 * @see sync_db_add_connection
 */
function sync_db_add_connection_submit($form, &$form_state) {  
  // Save away the current information.
  $current_step = 'step' . $form_state['storage']['step'];
  if (!empty($form_state['values'])) {
    $form_state['storage']['values'][$current_step] = $form_state['values'];
  }

  // Increment or decrement the step as needed. Recover values if they exist.
  if ($form_state['triggering_element']['#value'] == t('Next')) {
    $form_state['storage']['step']++;
    // If values have already been entered for this step, recover them from
    // $form_state['storage'] to pre-populate them.
    $step_name = 'step' . $form_state['storage']['step'];
    if (!empty($form_state['storage']['values'][$step_name])) {
      $form_state['values'][$step_name] = $form_state['storage']['values'][$step_name];
    }
  }
  if ($form_state['triggering_element']['#value'] == t('Previous')) {
    $form_state['storage']['step']--;
    // Recover our values from $form_state['storage'] to pre-populate them.
    $step_name = 'step' . $form_state['storage']['step'];
    $form_state['values'][$step_name] = $form_state['storage']['values'][$step_name];
  }

  // If they're done, submit.
  if ($form_state['triggering_element']['#value'] == t('Save')) {    
    sync_db_add_connection_final_submit($form, $form_state);
    $form_state['rebuild'] = FALSE;
    return;
  }

  // Otherwise, we still have work to do.
  $form_state['rebuild'] = TRUE;
}

/**
 * When form step achieves the max step number (aka last step), it will be called to
 * process the submit
 * 
 * @see sync_db_add_connection_submit
 */
function sync_db_add_connection_final_submit($form, &$form_state) {
  $step1 = $form_state['storage']['values']['step1'];
  $connection = new ConnectionInfo(
      $step1['conname'],
      $step1['dbname'],
      $step1['host'],
      $step1['username'],
      $step1['password'],
      $step1['driver']);
  
  $connection->setDescription($step1['description']);
  
  $table_array = array();
  $selected = array_filter($form_state['values']['tables']);
  foreach($selected as $tab) {
    $tabname = $form_state['values']['localname'][$tab];
    
    $aux = array(
      'real_name' => $tab,
      'local_name' => empty($tabname) ? $tab : $tabname,
      'condition' => $form_state['values']['condition'][$tab],
    );
    
    $table_array[] = $aux;
  }
  $connection->setTables($table_array);
  $connection->save();
  
  drupal_set_message(t('Connection !name saved succesfully', 
      array('!name' => $step1['conname'])));
  $form_state['redirect'] = sync_db_connection_path($connection);
  //TODO LOG saved connection
}

// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="Edit connection">

/**
 * Validates if the connection object actually exists, if true returns the form,
 * otherwise not found
 * 
 * @param ConnectionInfo $connection
 */
function sync_db_edit_connection_page($connection) {
  if ($connection instanceof ConnectionInfo) {
    return drupal_get_form('sync_db_edit_connection', $connection);
  }
  
  return MENU_NOT_FOUND;
}

/**
 * Builds the form for editing a connection
 *
 * @param ConnectionInfo $connection The connection being edited
 */
function sync_db_edit_connection($form, &$form_state, $connection) {
  $form = sync_db_build_connection_form($form_state);
  
  $form_state['storage']['connection'] = $connection;
  
  $step = $form_state['storage']['step'];
  switch($step) { 
    case 1:
      $form['driver'] = array(
        '#type' => 'item',
        '#title' => t('Driver'),
        '#markup' => $connection->getDriver(),
      );

      //If user went to step 2 and come back for the first one, then we will 
      //retrieve his populated data
      if (isset($form_state['storage']['values']['step1'])) {
        foreach($form as $key => &$field) {
          if (!empty($form_state['storage']['values']['step1'][$key]))
            $field['#default_value'] = $form_state['storage']['values']['step1'][$key];
        }
      }
      else { //First look at edit form
        $form['conname']['#default_value'] = $connection->getName();
        $form['dbname']['#default_value'] = $connection->getDatabase();
        $form['host']['#default_value'] = $connection->getHost();
        $form['username']['#default_value'] = $connection->getUsername();
        $form['password']['#default_value'] = $connection->getPassword();
        $form['description']['#default_value'] = $connection->getDescription();
      }
      break;
      
    case 2:
      $tables = $connection->getTables();
      $default_tabs = array();
      foreach ($tables as $tab) {
        $tablename = $tab['real_name'];
        $default_tabs[$tablename] = TRUE;
        $row = &$form['tables']['#options'][$tablename];
        $row['localname']['data']['#value'] = $tab['local_name'];
        $row['condition']['data']['#value'] = $tab['condition'];
      }

      $form['help']['#markup'] = t('Changing this options will affect this database!');
      $form['tables']['#default_value'] = $default_tabs;
      break;
    
    case 3:
      $form_state['redirect'] = sync_db_connection_path($connection);
      $form['warning'] = array(
        '#type' => 'item',
        '#title' => t('Warning'),
        '#markup' => t('These are the changes that will affect your local database. '
            . 'Last sync also will reset. Are you sure you want to proceed?')
      );
      
      $count = count($form_state['storage']['effects']['create']);
      $form['tables_to_create'] = array(
        '#type' => 'fieldset',
        '#title' => t('Tables to be CREATED') . " ($count)",
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      
          $items = array();
          foreach ($form_state['storage']['effects']['create'] as $name => $tab){
            $items[] = t("'!real_name' will be created as '!local_name'", array(
              '!real_name' => $name,
              '!local_name' => $tab['local_name']
            ));
          }
          $form['tables_to_create']['list_tables'] = array(
            '#theme' => 'item_list',
            '#items' => $items,
          );
      
      $count = count($form_state['storage']['effects']['rename']);
      $form['tables_to_rename'] = array(
        '#type' => 'fieldset',
        '#title' => t('Tables to be RENAMED')  . " ($count)",
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
      
          $items = array();          
          foreach ($form_state['storage']['effects']['rename'] as $name => $tab){
            //$original = $con->getTables()[$name]['real_name'];
            $before = $connection->getTables()[$name]['local_name'];
            $items[] = t("'!before_name' will be renamed to '!local_name'", array(
              '!before_name' => $before,
              '!local_name' => $tab['local_name']
            ));
          }
          $form['tables_to_rename']['list_tables'] = array(
            '#theme' => 'item_list',
            '#items' => $items,
          );
      
      $count = count($form_state['storage']['effects']['delete']);
      $form['tables_to_delete'] = array(
        '#type' => 'fieldset',
        '#title' => t('Tables to be DELETED') . " ($count)",
        '#collapsible' => TRUE,
        '#collapsed' => FALSE,
      );
      
          $items = array();
          foreach ($form_state['storage']['effects']['delete'] as $name => $tab){
            $items[] = t("'!local_name' will be deleted", array(              
              '!local_name' => $tab['local_name']
            ));
          }
          $form['tables_to_delete']['list_tables'] = array(
            '#theme' => 'item_list',
            '#items' => $items,
          );
      
      $form['confirm'] = array(
        '#type' => 'submit',
        '#value' => t('Confirm'),
      );
      
      //Putting the previous button in last place
      $prevbuton = $form['prev'];
      unset($form['prev']);
      $form['prev'] = $prevbuton;
  }
  
  $form['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => sync_db_connection_path($connection)
  );
  
  return $form;
}

/**
 * Form callback function for sync_db_edit_connection. While step variable doesnt achieve
 * last step, it will render form again
 * 
 * @see sync_db_edit_connection
 */
function sync_db_edit_connection_submit($form, &$form_state) {  
  $is_over = FALSE;
  // Save away the current information.
  $current_step = 'step' . $form_state['storage']['step'];
  if (!empty($form_state['values'])) {
    $form_state['storage']['values'][$current_step] = $form_state['values'];
  }

  // If it's next or save, it will increment a step
  if (in_array($form_state['triggering_element']['#value'], array(t('Next'), t('Save')))) {
    $form_state['storage']['step']++;
    // If values have already been entered for this step, recover them from
    // $form_state['storage'] to pre-populate them.
    $step_name = 'step' . $form_state['storage']['step'];
    if (!empty($form_state['storage']['values'][$step_name])) {
      $form_state['values'][$step_name] = $form_state['storage']['values'][$step_name];
    }
    
    //If we were at step 2 and incremented it, we will look for 3 now
    if ($form_state['storage']['step'] == 3) {
      sync_db_edit_connection_assess_effects($form_state);
      $is_over = TRUE;
      foreach ($form_state['storage']['effects'] as $effect) {
        if (!empty($effect))
          $is_over = FALSE;
      }
    }
  }
  if ($form_state['triggering_element']['#value'] == t('Previous')) {
    $form_state['storage']['step']--;
    // Recover our values from $form_state['storage'] to pre-populate them.
    $step_name = 'step' . $form_state['storage']['step'];
    $form_state['values'][$step_name] = $form_state['storage']['values'][$step_name];
  }

  if ($form_state['triggering_element']['#value'] == t('Confirm'))
    $is_over = TRUE;
  
  // If they're done, submit.
  if ($is_over) {
    sync_db_edit_connection_final_submit($form, $form_state);
    $form_state['rebuild'] = FALSE;
    return;
  }

  // Otherwise, we still have work to do.
  $form_state['rebuild'] = TRUE;
}

/**
 * It will be called when the edit form concludes its last step to process the form
 * submit
 */
function sync_db_edit_connection_final_submit($form, &$form_state) {
  /* @var $connection ConnectionInfo */
  $connection = $form_state['storage']['connection'];
  $step1 = $form_state['storage']['values']['step1'];
  
  $connection->setName($step1['conname']);
  $connection->setDatabase($step1['dbname']);
  $connection->setHost($step1['host']);
  $connection->setUsername($step1['username']);
  $connection->setPassword($step1['password']);
  $connection->setDescription($step1['description']);
  
  $form_state['redirect'] = sync_db_connection_path($connection);
  
  //I already set new table selection in sync_db_edit_connection_assess_effects()
  $connection->save();
    drupal_set_message(t('Changes to connection !name applied and saved succesfully', 
      array('!name' => $connection->getName())));
}

/**
 * Assess the effects that will be done on database for the edit connection form 
 * and store its effects on form_state[storage][effects]
 */
function sync_db_edit_connection_assess_effects(&$form_state) {
  /* @var $con ConnectionInfo */
  $con = $form_state['storage']['connection'];
  $filtered = array_filter($form_state['storage']['values']['step2']['tables']);
  
  $old_selection = $con->getTables();
  $new_selection = array();
  
  foreach($filtered as $tab) {
    $localname = $form_state['values']['localname'][$tab];    
        
    $aux = array (
      'real_name' => $tab,
      'local_name' => empty($localname) ? $tab : $localname,
      'condition' => $form_state['values']['condition'][$tab],
    );
    
    if (in_array($tab, $old_selection)) {
      $aux['tid'] = $old_selection[$tab]['tid'];
    }
    
    $new_selection[$tab] = $aux;
  }  
  
  //Since it's not commited until save() function is called, let's store it already
  $con->setTables($new_selection);  
  $to_delete = array();
  foreach ($old_selection as $tabindex => $tab) {
    if (!isset($new_selection[$tabindex])) {//This table were unchecked
      $to_delete[$tabindex] = $tab;
    }
  }
  
  $to_rename = array();
  $to_create = array();
  foreach ($new_selection as $tabindex => $tab) {
    if (isset($old_selection[$tabindex])) { //This table selection did'nt change
      if ($tab['local_name'] != $old_selection[$tabindex]['local_name']) //This table's local name changed
        $to_rename[$tabindex] = $tab;
    }
    else { //A new table were selected
      $to_create[$tabindex] = $tab;
    }
  }
  
  $form_state['storage']['effects']['create'] = $to_create;
  $form_state['storage']['effects']['rename'] = $to_rename;
  $form_state['storage']['effects']['delete'] = $to_delete;
}
// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="Delete connection">

/**
 * Validates if the conection object actually exists, if so it returns the delete form,
 * otherwise not found
 * 
 * @param ConnectionInfo $connection The connection object that you want to delete
 */
function sync_db_delete_connection_page($connection) {
    if ($connection instanceof ConnectionInfo) {
    return drupal_get_form('sync_db_delete_connection', $connection);
  }
  
  return MENU_NOT_FOUND;
}

/**
 * Builds the form for deleting a connection
 *
 * @param ConnectionInfo $connection The connection to delete
 */
function sync_db_delete_connection($form, &$form_state, $connection) {
  
  $form_state['storage']['connection'] = $connection;
  $form['warning'] = array (
    '#type' => 'item',
    '#title' => t('Are you sure?'),
    '#markup' => t('If you delete this connection the following data will be lost')
  );
  
  $items = array();
  foreach ($connection->getTables() as $tab) {
    $items[] = t('!tabname table will be deleted with all its data',
        array('!tabname' => $tab['local_name']));
  }
  $form['delete-list'] = array (
    '#theme' => 'item_list',
    '#items' => $items
  );
  
  $form['imwarned'] = array(
    '#required' => TRUE,
    //'#input' => TRUE,
    '#type' => 'checkbox',
    //'#return_value' => TRUE,
    //'#process' => array('form_process_checkbox', 'ajax_process_form'),
    '#title' => t('I read and I am warned of the effects of this action'),
    //'#theme' => 'form_example_checkbox',
    //'#theme_wrappers' => array('form_element'),
    //'#title_display' => 'after',    
  );
  
  $form['submit'] = array (
    '#type' => 'submit',
    '#value' => t('Confirm')
  );
  
  return $form;
}

/**
 * Submit function for delete connection form
 * @see sync_db_delete_connection()
 */
function sync_db_delete_connection_submit($form, &$form_state) {
  
  /* @var $connection ConnectionInfo */
  $connection = $form_state['storage']['connection'];
  
  $connection->delete();
  
  drupal_set_message(t('Connection !conname deleted successfully', 
      array('!conname' => $connection->getName())));
  //TODO LOG delete action
}
// </editor-fold>

// <editor-fold desc="Form validation functions">

/**
 * Form validation function that checks if the desired name for the connection 
 * isn't in use by any other connection
 * 
 * @see sync_db_connection_exists
 */
function sync_db_connection_name_validate($form, &$form_state) {
  $name = strtolower($form_state['values']['conname']);
  
  if ($name == 'testing') {
    form_set_error('conname', t("'testing' isn't available for naming connections."));
    return FALSE;
  }
  
  if (strpos($name, '-') !== FALSE) {
    form_set_error('conname', t("'-' not allowed in the name"));
    return FALSE;
  }
  
  if ($form_state['build_info']['form_id'] == 'sync_db_edit_connection') { //Editing an already existing connection
    $con = $form_state['storage']['connection'];
    if ($con->getName() == $form_state['values']['conname'])
      return TRUE; //There's not need to validate it since it hasn't changed.
  }
  
  if (sync_db_connection_exists($name)) {
    form_set_error('conname', t("Another connection is already using this name."));
    return FALSE;
  }
  
  return TRUE;
}

/**
 * Form validation function that performs a connection and a query on the database
 * to check if its okay
 * 
 * @see sync_db_test_connection
 */
function sync_db_try_connect_validate($form, &$form_state) {
  $values = $form_state['values'];
  
  if ($form_state['build_info']['form_id'] == 'sync_db_edit_connection') {
    $driver = $form_state['storage']['connection']->getDriver();
  }
  else {
    $driver = $values['driver'];
  }
  
  $testResult = sync_db_test_connection(
      $values['dbname'], 
      $values['host'], 
      $values['username'], 
      $values['password'], 
      $driver);
  
  if (!$testResult) {
    form_set_error('dbname', 'Connection failed');
    return FALSE;
  }
  
  return TRUE;
}

/**
 * Form validation function that validates if the checked tables and its options
 * are okay
 */
function sync_db_selected_tables_validate($form, &$form_state) {
  
  $selected = array_filter($form_state['values']['tables']);
  $assignedNames = $form_state['values']['localname'];
  
  if (empty($selected)) {
    form_set_error('tables', t('You should select at least one table'));
    return FALSE;
  }
  
  $form_id = $form_state['build_info']['form_id'];  
  $con_id = $form_id == 'sync_db_edit_connection' ? 
      $form_state['storage']['connection']->getId() : FALSE;

  foreach ($selected as $table) {
    $nameToCheck = (empty($assignedNames[$table])) ? $table : $assignedNames[$table];
    
    if ($form_id == 'sync_db_edit_connection' && isset($form_state['storage']['connection']->getTables()[$table])) {
      $compareName = $form_state['storage']['connection']->getTables()[$table];
      if ($compareName['local_name'] == $nameToCheck) //Since it hasn't changed, doesn't need to validate
        continue;      
    }
    
    if (db_table_exists($nameToCheck)) {
      form_set_error('tables', t("Table '!name' already exists in this database. Please, assign another name.", array(
        '!name' => $nameToCheck
      )));
      return FALSE;
    }
    
    
    if (ConnectionInfo::isTableNameInUse($nameToCheck, $con_id)) {
      form_set_error('tables', t("Table '!name' already in use by another connection. Please, assign another name.", array(
        '!name' => $nameToCheck
      )));
      return FALSE;
    }
  }
}

// </editor-fold>

// <editor-fold defaultstate="collapsed" desc="Repair connection">

/**
 * 
 * @param ConnectionInfo $connection
 * @see sync_db_repair_connection()
 */
function sync_db_repair_connection_page($connection) {
  if ($connection instanceof ConnectionInfo) {
    return drupal_get_form('sync_db_repair_connection', $connection);
  }
  
  return MENU_NOT_FOUND;
}

/**
 * Check structure gaps and repair tables' fields
 * 
 * @param ConnectionInfo $connection
 * @return string
 */
function sync_db_repair_connection($form, &$form_state, $connection) {
  $form_state['storage']['connection'] = $connection;
  
  $form['description'] = array(
    '#type' => 'markup',
    '#markup' => '<p>' . t("Perfom a repair on tables' structure if there is any mismatched fields.") . '</p>'
  );
  
  $form['verify'] = array(
    '#id' => 'verify-sync-db-structure',
    '#type' => 'submit',
    '#value' => t('Check for structure gaps'),
    '#ajax' => array(      
      'wrapper' => 'report-fieldset-wrapper',
      'callback' => 'sync_db_repair_connection_verify_callback'
    ),
  );
  
  $form['report'] = array(
    '#type' => 'markup',
    '#prefix' => '<div id="report-fieldset-wrapper">',
    '#suffix' => '</div>',
  );
  
  if (isset($form_state['storage']['repair'])) {
    $form['report'] = array(
      '#type' => 'fieldset',
      '#title' => t('Report'),
      '#prefix' => '<div id="report-fieldset-wrapper">',
      '#suffix' => '</div>',
    );
    
    if (!$form_state['storage']['repair']['result']) {
      
      $report = $form_state['storage']['report'];
      
      /*$form['report']['debug'] = array(
        '#type' => 'markup',
        '#markup' => '<pre>'.print_r($form_state['storage']['report'], 1).'</pre>'
      );*/
     
      $count = count($report['create']);
      if ($count > 0) {
        $form['report']['tables_to_create'] = array(
          '#type' => 'fieldset',
          '#title' => t('Tables to be CREATED') . " ($count)",
          '#collapsible' => TRUE,
          '#collapsed' => TRUE,
        );

          $form['report']['tables_to_create']['list_tables'] = array(
            '#theme' => 'item_list',
            '#items' => $report['create'],
          );
      }
      
      //$count = count($report['change'];
      if (count($report['change']) > 0) {

        $items = array();
        $bold_span_open_tag = '<span style="font-weight: bold; ">';
        $italic_span_open_tag = '<span style=" font-style: italic; ">';
        foreach ($report['diff'] as $tabname => $structure) {
          $count = count($structure['fields']);
          if (isset($structure['create']))
            $count += count($structure['create']);
          $form['report']['changes'][$tabname] = array(
            '#type' => 'fieldset',
            '#title' => t('Table !tabname: !count change(s)', array('!tabname' => $tabname, '!count' => $count)),
            '#collapsible' => TRUE,
            '#collapsed' => FALSE,
          ); 
      
          $items = array();
          foreach ($structure['fields'] as $field => $spec) {
            $details = '';
            foreach ($spec as $attr => $val) {
              $details .= $bold_span_open_tag . $attr .'</span>'. t(' to ') . $italic_span_open_tag . $val . '</span>, ';
            }
            $details = substr($details, 0, strlen($details) -9);
            
            $items[] = t("!field will be modified: !details", array(
              '!field' => $bold_span_open_tag . $field . '</span>',
              '!details' => $details
            ));
          }
          
          if (isset($structure['create'])) {
            foreach ($structure['create'] as $missing) {
              $items[] = t("!field is missing and will be created", array(
                  '!field' => $bold_span_open_tag . $missing . '</span>'
                ));
            }
          }
          
          $form['report']['changes'][$tabname]['change-list'] = array(
            '#theme' => 'item_list',
            '#items' => $items,
          );
        }
      }
      
      $form['report']['repair'] = array(
        '#type' => 'submit',
        '#id' => 'sync-db-repair-submit',
        '#value' => t('Repair structure')
      );
    }
    else {
      $form['report']['result'] = array(
        '#type' => 'markup',
        '#markup' => t("Everything's okay around here.")
      );
    }
  }
  
  return $form;
}

/**
 * Submit callback for sync_db_repair_connection
 * 
 * @see sync_db_repair_connection
 */
function sync_db_repair_connection_submit($form, &$form_state) {
  if ($form_state['triggering_element']['#id'] == 'verify-sync-db-structure') {
    $con = $form_state['storage']['connection'];
    $repair = RepairSyncDB::createFromConnection($con);
    $form_state['storage']['repair']['object'] = $repair;
    $form_state['rebuild'] = TRUE;
    
    try {
      if (!$form_state['storage']['repair']['result'] = $repair->verify()) {
        $form_state['storage']['report'] = $repair->getReport(); 
      }
    }
    catch (SyncDB_Exception $ex) {
      drupal_set_message($ex->getMessage(), 'error');
      $form_state['rebuild'] = FALSE;
    }
  }
  else {//We're done
    /** @var $repair RepairSyncDB */
    $repair = $form_state['storage']['repair']['object'];
      
    $repair->repair();    
    
    $form_state['rebuild'] = FALSE;
    drupal_set_message(t('Connection was succesfully repaired'));
  }
}

/**
 * Callback for verify tables' structure
 */
function sync_db_repair_connection_verify_callback($form, $form_state) {
  return $form['report'];
}

//</editor-fold>