<?php

/**
 * A class that encompasses all specific database queries used by sync db module
 * 
 * @author Guilherme
 */
abstract class SyncDB_Schema {
  
  /**
   * @var ConnectionInfo
   */
  protected $connection;
  
  /**
   * @param ConnectionInfo $connection
   */
  function __construct(ConnectionInfo $connection) {
    $this->connection = $connection;
  }
  
  /**
   * Returns the ConnectionInfo object which is being used by its schema
   * @return ConnectionInfo
   */
  function getConnectionInfo() {
    return $this->connection;
  }
  
  /**
   * Return a string query to test the connection
   * 
   * @return String The string to perform a test query on database
   */
  function getTestQuery() {
    return 'show tables';
  }
  
  /**
   * Gets the corresponding datatype field in Drupal and change it to become 
   * supportable by module if necessary 
   * 
   * @param Array $field An array with the field structure to be created
   */
  function getFieldType($field, $supress_warnings = FALSE) {
    $map = $this->getFieldTypeMap();
    $type = strtoupper($field['type']);
    
    if (isset($map[$type])) {
      $type_ex = explode(':', $map[$type]);
      $field['type'] = $type_ex[0];
      $field['size'] = $type_ex[1];
    }
    else {
      if (!$supress_warnings)
        drupal_set_message(t('Type !type not supported in !table.!field ("varchar(150)" assumed).', array(
          '!type' => $type,
          '!table' => $field['table'],
          '!field' => $field['name'],
        )), 'warning');
      $field['type'] = 'varchar';
      $field['length'] = 150;
    }
    return $field;
  }
  
  /**
   * Returns all primary keys of the tables passed as arg
   * 
   * @param Array $tables An array with all table names (in local database)
   * @return Array All primary keys of the tables
   */
  abstract function getPrimaryKeys($tables);
  
  /**
   * Returns all existing tables in this database
   * 
   * @return Array An array with all tables
   */
  abstract function getTables();
  
  /**
   * Returns the structure of all passed tables in that database
   * 
   * @param string $database The database name
   * @param array $tables An array with all tables' names
   * @param boolean $supress_warnings Whether to supress warnings
   * @param string $key The key in array index to get the tables' name
   * @return array An array with all passed by tables with their each structure
   */
  abstract function getTableStructure($database, $tables, $supress_warnings = TRUE, $key = 'real_name');
  
  protected abstract function getFieldTypeMap();
}
