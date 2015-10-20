<?php

/**
 * @file
 * Handles all actions related to the structure of a table
 */

/**
 * A class that can repair and compare two table structures from different databases
 *
 * @author Guilherme
 */
class SyncTableStructure {
  
  /**
   * @var ConnectionInfo 
   */
  protected $connection;
  /**
   * @var SyncDB_Schema
   */
  protected $schema;
  
  /**
   * 
   * @param SyncDB_Schema $schema
   */
  function __construct(SyncDB_Schema $schema) {
    $this->schema = $schema;
    $this->connection = $schema->getConnectionInfo();
  }
  
  /**
   * Gets the connection being used by this
   * @return ConnectionInfo
   */
  function getConnectionInfo() {
    return $this->connection;
  }

  /**
   * Gets the SyncDB_Schema being used by this
   * @return SyncDB_Schema
   */
  function getSchema() {
    return $this->schema;
  }

  /**
   * Repairs the tables changing its fields to the correct format
   * 
   * @param Array $torepair An array with which fields may be changed
   */
  function repair($torepair) {
    foreach ($torepair as $local_table => $structure) {
      
      if (!db_table_exists($local_table)) {        
        db_create_table($local_table, $structure);
      }
      else {
        foreach ($structure['fields'] as $field => $spec) {
          if (!db_field_exists($local_table, $field)) {
            db_add_field($local_table, $field, $spec);
          }
          else {
            db_change_field($local_table, $field, $field, $spec);
          }
        }      
      }
    }
  }
  
  /**
   * Verifies the current structure against the ConnectionInfo's database and
   * generates a report
   * 
   * @param boolean $supress_warnings Whether the warnings should be supressed
   * @return Array A report with all differences between both structures
   */
  function verify($supress_warnings = FALSE) {
    $con = $this->connection;
    try {      
      $to_check = $con->getTables();

      //Getting third-party database structure
      $con->setAsActiveConnection();
      $third_structure = $this->schema->getTableStructure($con->getDatabase(), $to_check, $supress_warnings);
      $con->setActiveConnectionToDefault();

      //Getting local structure
      $db_local_name = ConnectionInfo::getLocalDatabase();
      $local_structure = $this->schema->getTableStructure($db_local_name, $to_check, TRUE, 'local_name');

      //Setting up report array
      $report['create'] = array();
      $report['change'] = array();
      $report['verifiedcount'] = 0;
      $report['diff'] = array();
      $report['repair'] = array();

      //Generating report
      foreach ($to_check as $tab) {
        $real_name = $tab['real_name'];
        $local_name = $tab['local_name'];

        if (!db_table_exists($local_name)) {
          $report['create'][] = $local_name;
          $report['repair'][$local_name] = $third_structure[$real_name];
        }
        elseif ($local_structure[$local_name] != $third_structure[$real_name]) {
          $report['change'][] = $local_name;
          $report['diff'][$local_name] = 
              $this->arrayMultiDiff($local_structure[$local_name], $third_structure[$real_name]);

          foreach ($report['diff'][$local_name]['fields'] as $field_name => $field) {
            $report['repair'][$local_name]['fields'][$field_name] = $third_structure[$real_name]['fields'][$field_name];
          }

          //Checked only existing fields, let's look for missing fields
          foreach ($third_structure[$real_name]['fields'] as $field_name => $field_struct) {
            if (!isset($local_structure[$local_name]['fields'][$field_name])) {//Doesn't exists            
              $report['repair'][$local_name]['fields'][$field_name] = $field_struct;
              $report['diff'][$local_name]['create'][] = $field_name;
            }
          }
        }

        $report['verifiedcount']++;
      }
    }
    catch (PDOException $ex) {
      $con->setActiveConnectionToDefault();
      switch ($ex->getCode()) {
          case 1044: //Login not exists
          case 1045: //Login and pass doesnt match
            $this->connection->setActiveConnectionToDefault();
            throw new InvalidCredentialsException(
                $this->getConnectionInfo()->getUsername(),
                $this->getConnectionInfo()->getDatabase(),
                t("Can't get data from database. Check your credentials."),
                $ex->getCode(),
                $ex);
            
          case 2002:
            $this->connection->setActiveConnectionToDefault();
            throw new ServerNotRespondException(
                $this->getConnectionInfo()->getHost(),
                "Server doesn't responds",
                $ex->getCode(),
                $ex);
            
          default:
            throw new SyncDB_Exception(t("Couldn't verify the structure: !exp", array(
              '!exp' => $ex->getMessage()
            )), $ex->getCode(), $ex);
      }
    }
    
    return $report;
  }
  
  /**
   * Returns the difference between two arrays. It's a multidimensional array version of
   * the php native function 
   * @see array_diff()
   * 
   * @param Array $arr1
   * @param Array $arr2
   */
  protected function arrayMultiDiff($arr1, $arr2) {
    $result = array();
    foreach ($arr1 as $k => $v) {
      if(!isset($arr2[$k])){
        $result[$k] = $v;
      } else {
        if(is_array($v) && is_array($arr2[$k])){
          $diff = $this->arrayMultiDiff($v, $arr2[$k]);
          if(!empty($diff))
            $result[$k] = $diff;
        }
        else {
          if ($v != $arr2[$k]) {
            $result[$k] = $arr2[$k];
          }
        }
      }
    }
    return $result;
  }
}

/**
 * A facade class of SyncTableStructure to perform repair and verify
 */
class RepairSyncDB {
  
  /**
   * @var Array the report returned by verification
   */
  private $report = array();
  /**
   * @var Array The difference between the two structures
   */
  private $structure_diff = array();
  /**
   * @var SyncTableStructure
   */
  private $table_structure;
  
  /**
   * @param SyncTableStructure $table_structure
   */
  function __construct(SyncTableStructure $table_structure) {
    $this->table_structure = $table_structure;
  }
  
  /**
   * Creates and returns a RepairSyncDB instance by a ConnectionInfo
   * 
   * @param ConnectionInfo $connection
   * @return \RepairSyncDB
   */
  static function createFromConnection(ConnectionInfo $connection) {
    return
      new RepairSyncDB(new SyncTableStructure($connection->getSchema()));
  }

  /**
   * Returns the report generated by verification of the structure
   * 
   * @return Array Verification's report
   */
  function getReport() {
    return $this->report;
  }

  /**
   * Repairs the tables changing its fields to the correct format
   */
  function repair() {
    if (!isset($this->getReport()['repair'])) {
      if ($this->verify())
        return; //No need for repair
    }
    
    $report = $this->getReport();
    
    //Checking for any nullable fields that may trigger errors. Constraint set in ➡R.009
    $rowscount = $this->table_structure->getConnectionInfo()->getTablesRowCount();
    foreach ($report['diff'] as $tabname => $structure) {
      
      if (isset($structure['fields'])) {
        foreach ($structure['fields'] as $fieldname => $field) {
          if (isset($field['not null']) && $rowscount[$tabname] > 0) {
            $count_null = db_select($tabname, 'generic')
                ->fields('generic')
                ->condition($fieldname)
                ->execute()
                ->rowCount();
            
            if ($count_null > 0) {
              unset($report['repair'][$tabname]['fields'][$fieldname]['not null']);
              drupal_set_message(t("The field !field cannot be set null because "
                  . "there are !nullcount row(s) where !field is null. "
                  . "!sync or truncate the table and try to repair it again later.", 
                  array(
                    '!field' => $fieldname,
                    '!nullcount' => $count_null,
                    '!sync' => l(t('Sync'), sync_db_connection_path($this->table_structure->getConnectionInfo(), 'sync')),
                )), 'warning');
            }
          }
        }
      }
      
      if (isset($structure['create'])) {
        foreach ($structure['create'] as $fieldname) {
          if (isset($report['repair'][$tabname]['fields'][$fieldname]['not null']) && $rowscount[$tabname] > 0) {
            unset($report['repair'][$tabname]['fields'][$fieldname]['not null']);
            drupal_set_message(t("The field !field was created as a nullable field because it has at least one record. "
                . "!sync or truncate the table and try to repair it again later.", array(
                  '!field' => $fieldname,
                  '!sync' => l(t('Sync'), sync_db_connection_path($this->table_structure->getConnectionInfo(), 'sync')),
                )), 'warning');
          }
        }
      }
    }
    
    
    $this->table_structure->repair($report['repair']);
  }
  
  /**
   * Verifies both structures and generates a report with differences
   * 
   * @param boolean $supress_warnings Whether the warnings should be supressed
   * @return boolean Whether the structures are identical or not
   */
  function verify($supress_warnings = FALSE) {
    $this->report = $this->table_structure->verify($supress_warnings);
    
    if (!empty($this->report['create'])) {
      return FALSE;
    }
    
    if (!empty($this->report['change'])) {
      return FALSE;
    }
    
    return TRUE;
  }
}