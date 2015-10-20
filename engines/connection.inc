<?php

/**
 * A class that store all required information to perfom a connection.
 * It handles the database queries to save any changes of the connection and handles
 * Drupal's functions
 *
 * @author Guilherme
 */
class ConnectionInfo {
  
  // <editor-fold defaultstate="collapsed" desc="Properties">
  /**
   * The primary key on the database 
   * 
   * @var int
   */
  private $id;
  /**
   * The primary identifies name for the connection
   * 
   * @var string
   */
  private $name;
  /**
   * The database name
   * 
   * @var string 
   */
  private $database;
  /**
   * An optional description about the current connection
   * @var string 
   */
  private $description;
  
  /**
   * The database's host
   * 
   * @var string 
   */
  private $host;
  /**
   * The username credential used for connecting on the database
   * 
   * @var string 
   */
  private $username;
  /**
   * The password credential used for connecting on the database
   * 
   * @var string 
   */
  private $password;
  /**
   * The unix timestamp of the last time that this current connection was sync
   * 
   * @var string 
   */
  private $last_sync;
  /**
   * The database driver (e.g. mysql, postgree)
   * 
   * @var string 
   */
  private $driver;
  
  /**
   * The sync schema object used for specific queries
   * 
   * @var SyncDB_Schema 
   */
  private $schema;
  
  /**
   * An associative array with all chosen tables for sync. 
   * The array is indexed by the real_name table's
   * 
   * @var array 
   */
  private $tables = array();
  
  /**
   * An associative array with all tables that must be updated or added when save function is called.
   * When the connection is saved, this one becames the $tables array
   * 
   * @see ConnectionInfo::save()
   * 
   * @var array 
   */
  private $unsaved_tables = array();
  
  //</editor-fold>
  
  function __construct($name, $database, $host, $username, $password, $driver) {
    $this->setName($name);
    $this->setDatabase($database);
    $this->setHost($host);
    $this->setUsername($username);
    $this->setPassword($password);
    $this->setDriver($driver);
  }
  
  // <editor-fold defaultstate="collapsed" desc="Get methods">
  
  /**
   * Returns the code id for the connection
   * @return int
   */
  function getId() {
    return $this->id;
  }

  /**
   * Returns the connection name
   * @return string
   */
  function getName() {
    return $this->name;
  }

  /**
   * Returns the database name
   * @return string
   */
  function getDatabase() {
    return $this->database;
  }

  /**
   * Returns the database's host
   * @return string
   */
  function getHost() {
    return $this->host;
  }

  /**
   * Returns the username used for login
   * @return string
   */
  function getUsername() {
    return $this->username;
  }

  /**
   * Returns the password used for login
   * @return string
   */
  function getPassword() {
    return $this->password;
  }

  /**
   * Returns the unix timestamp of the last time that this connection was synchronized
   * @return int
   */
  function getLastSync() {
    return $this->last_sync;
  }

  /**
   * Returns the SyncDB_Schema object used for doing specific queries
   * 
   * @return SyncDB_Schema
   */
  function getSchema() {
    return $this->schema;
  }
  
  /**
   * Returns the description of this connection
   * @return string
   */
  function getDescription() {
    return $this->description;
  }
  
  /**
   * Returns the database's driver (e.g. mysql, postgree)
   * @return string
   */
  function getDriver() {
    return $this->driver;
  }
  
  /**
   * Returns an associative array with the selected tables for sync.
   * The tables that were saved.
   * 
   * @return array
   */
  function getTables() {
    return $this->tables;
  }
  
    
  /**
   * Returns the associative name of the table in local database
   * @param String $original_name The original table name
   */
  function getLocalTableName($original_name) {
    if (isset($this->getTables()[$original_name]))
      return $this->getTables()[$original_name]['local_name'];
    
    return FALSE;
  }
  
  /**
   * Returns the row count for each table
   * @return An array indexed by tables' name (third-party) with the row count in each one
   */
  function getTablesRowCount() {
    $tabs = $this->getTables();
    
    $counts = array();
    foreach ($tabs as $name => $details) {
      $local = $details['local_name'];
      $counts[$name] = (db_table_exists($local)) ? 
        db_query("select * from $local")->rowCount() : -1;      
    }
    return $counts;
  }
  
  /**
   * Returns the default database set in Drupal.
   * @return String
   * @see Database::getConnectionInfo()
   */
  static function getLocalDatabase() {
    return Database::getConnectionInfo()['default']['database'];
  }

  //</editor-fold>
  
  // <editor-fold defaultstate="collapsed" desc="Set methods">
  function setName($name) {
    $this->name = strtolower($name);
  }

  function setDatabase($database) {
    $this->database = $database;
  }

  function setHost($host) {
    $this->host = $host;
  }

  function setUsername($username) {
    $this->username = $username;
  }

  function setPassword($password) {
    $this->password = $password;
  }

  function setDriver($driver) {
    
    if ($this->driver != $driver){      
      $this->driver = $driver;

      switch ($driver) {
        case 'mysql':
          $this->schema = new SyncDB_MySQL($this);
          break;
      }
    }
  }
  
  function setDescription($description) {
    $this->description = $description;
  }
  
  function setLastSyncToNow() {
    $this->last_sync = time();
  }
  
  /**
   * The tables are stored in a temporary array. When save function is called, the
   * changes are applied.
   * 
   * @see ConnectionInfo::save()
   * 
   * @param array $table_array An associative array with all new chosen tables
   */
  function setTables($table_array) {
    $this->unsaved_tables = $table_array;
  }
  //</editor-fold>

  // <editor-fold desc="Drupal related functions">
  
  /**
   * Adds connection information to Drupal. Necessary for establishing a connection.
   */
  function addToDrupal() {
    Database::addConnectionInfo($this->getName(), 'default', $this->toArray());
  }
  
  /**
   * Removes connection information from Drupal
   */
  function removeFromDrupal() {
    Database::removeConnection($this->getName());
  }
  
  /**
   * Returns an array with database information
   * @return array
   */
  function toArray() {
    return array(
      'database' => $this->getDatabase(),
      'username' => $this->getUsername(),
      'password' => $this->getPassword(),
      'host' => $this->getHost(),
      'driver' => $this->getDriver(),
    );
  }
  
  /**
   * Set this as the active connection on Drupal. Necessary before doing queries.
   * Drupal must have information about the connection before using this.
   * 
   * @see addToDrupal()
   */
  function setAsActiveConnection() {
    $this->addToDrupal();
    Database::setActiveConnection($this->getName());
  }
  
  /**
   * Returns the default database as the active one used by Drupal.
   */
  function setActiveConnectionToDefault() {
    Database::setActiveConnection();
  }
  
  // </editor-fold>
  
  // <editor-fold defaultstate="collapsed" desc="Database related functions">
  
  /**
   * Creates or updates the connection on the database
   */
  function save() {    
    
    $transaction = db_transaction();
    try {
      if (is_null($this->getId())) {//INSERT      
        $id = db_insert('sync_db_connection')
            ->fields(array(
              'name' => $this->getName(),
              '`database`' => $this->getDatabase(),
              'host' => $this->getHost(),
              'username' => $this->getUsername(),
              'password' => $this->getPassword(),
              'driver' => $this->getDriver(),
              'description' => $this->getDescription()
            ))->execute();

        $this->id = $id;

        foreach($this->unsaved_tables as &$tab){
          $id = db_insert('sync_db_table')
              ->fields(array(
                'cid' => $this->getId(),
                'real_name' => $tab['real_name'],
                'local_name' => $tab['local_name'],
                '`condition`' => $tab['condition']
              ))->execute();

          $tab['id'] = $id;
        }

        //CREATE TABLES' STRUCTURE
        $this->setAsActiveConnection();
        $structs = $this->getSchema()->getTableStructure($this->getDatabase(), $this->unsaved_tables);
        $this->setActiveConnectionToDefault();        
        
        foreach($structs as $tab => $body) {
          db_create_table($tab, $body);
        }
      } 
      else { //UPDATE           

        $old = $this->tables;
        $new = $this->unsaved_tables;

        foreach($old as $key => $tab) {
          if (isset($new[$key])) {
            $new[$key]['tid'] = $tab['tid'];//Copys ID
            if ($tab['local_name'] != $new[$key]['local_name']) { 
              //Table's local name has been changed, update it on database!
              db_rename_table($tab['local_name'], $new[$key]['local_name']);
              
            }
          } 
          else {//It has been removed and should be deleted
            db_drop_table($tab['local_name']);
          }
        }
        
        $this->setAsActiveConnection();
        $structs = $this->getSchema()->getTableStructure($this->getDatabase(), $new);
        $this->setActiveConnectionToDefault();

        foreach($new as $key => $tab) {
          if(!isset($old[$key])) {//So, it should be created
            db_create_table($tab['local_name'], $structs[$key]);
            $this->last_sync = 0;
          }
        }
        
        db_update('sync_db_connection')
          ->fields(array(
            'name' => $this->getName(),
            '`database`' => $this->getDatabase(),
            'host' => $this->getHost(),
            'username' => $this->getUsername(),
            'password' => $this->getPassword(),
            'last_sync' => $this->getLastSync(),
            'description' => $this->getDescription()
          ))
          ->condition('cid', $this->getId())
          ->execute();

        //Deleting unselected tables
        foreach ($old as $key => $tab) {
          if (!isset($new[$key])) {
            db_delete('sync_db_table')
              ->condition ('tid', $tab['tid'])
              ->execute();
          }
        }
        
        //Updating and insert tables' entries on database
        foreach ($new as $key => $tab) {
          //To avoid errors to sql command
          $tab['`condition`'] = $tab['condition'];
          unset($tab['condition']);
          $tab['cid'] = $this->getId();
          
          if (isset($old[$key])) { //Update          
            $tid = $old[$key]['tid'];
            //Unnecessary fields for updating table
            unset($tab['structure']);
            unset($tab['tid']);

            db_update('sync_db_table')
              ->fields($tab)
              ->condition('tid', $tid)
              ->execute();
          } 
          else {//Insert
            //Unnecessary fields for updating table
            unset($tab['structure']);

            db_insert('sync_db_table')
              ->fields($tab)
              ->execute();
          }
        }        
        
        $this->tables = $new;
        $this->unsaved_tables = NULL;
      }
    }
    catch (Exception $ex) {
      $transaction->rollback();
      throw $ex;
    }
  }
  
  /**
   * Deletes its entry and all associated entries from database
   */
  function delete() {
    //If exists its entry on database
    if ($this->getId()) {
      
      $trans = db_transaction();
      
      try {
        //Delete associated tables
        foreach($this->getTables() as $tab) {
          if (db_table_exists($tab['local_name']))
            db_drop_table ($tab['local_name']);
        }

        //Delete database's entries
        db_delete('sync_db_table')
          ->condition('cid', $this->getId())
          ->execute();

        db_delete('sync_db_connection')
          ->condition('cid', $this->getId())
          ->execute();
        
        $this->removeFromDrupal();
      } catch (Exception $ex) {
        $trans->rollback();
        drupal_set_message(t('Occurred an error while trying to delete a connection: !error', array('!error', $ex->getMessage())), 'error');
      }
    }
  }
  
  /**
   * Returns a Connection Info Object by its id, otherwise returns FALSE
   * 
   * @param int $id
   * @return \ConnectionInfo
   */
  static function getConnectionInfoById($id) {
    $query = db_select('sync_db_connection', 'con')
        ->fields('con')
        ->condition('cid', $id)
        ->range(0, 1);
    
    return 
      ConnectionInfo::getConnectionInfo($query);
  }
  
  /**
   * Returns a Connection Info Object by its name, otherwise returns FALSE
   * 
   * @param string $name
   * @return \ConnectionInfo
   */
  static function getConnectionInfoByName($name) {
    $query = db_select('sync_db_connection', 'con')
        ->fields('con')
        ->condition('name', $name)
        ->range(0, 1);
    
    return 
      ConnectionInfo::getConnectionInfo($query);
  }
  
  /**
   * Returns a ConnectionInfoObject from a SelectQuery result
   * 
   * @param SelectQuery $query
   */
  private static function getConnectionInfo($query) {
    $result = $query->execute()->fetch(PDO::FETCH_ASSOC);    
    
    if ($result) {
      $con = new ConnectionInfo(
          $result['name'], 
          $result['database'], 
          $result['host'], 
          $result['username'], 
          $result['password'], 
          $result['driver']);
      
      $con->id = $result['cid'];
      $con->last_sync = $result['last_sync'];
      $con->setDescription($result['description']);
      
      $tabquery = db_select('sync_db_table', 'tab')
          ->fields('tab')
          ->condition('cid', $result['cid'])
          ->execute();
      
      $con->tables = array();
      while($row = $tabquery->fetch(PDO::FETCH_ASSOC))
        $con->tables[$row['real_name']] = $row;
      
      return $con;
    }
    
    return FALSE;
  }

  /**
   * Returns the id of a connection by its name
   * @param String $name
   * @return int
   */
  static function getConnectionIDByName($name) {
    return
    db_select('sync_db_connection', 'con')
      ->fields('con', array('cid'))
      ->condition('name', $name)
      ->range(0, 1)
      ->execute()
      ->fetchField();
  }
  
  /**
   * Checks if a name is being used by some connection
   * 
   * @param string $name The name of the table
   * @param int $not_id (Optional) An connection id to ignore when checking
   * @return boolean Whether a connection uses this table or not
   */
  static function isTableNameInUse($name, $not_id = FALSE) {
    $query = db_select('sync_db_table', 'tab')
        ->fields('tab', array('tid', 'cid'))
        ->condition('local_name', $name);
    
    if ($not_id) {
      $query->condition('cid', $not_id, '!=');
    }
    
    if ($query->execute()->rowCount() > 0)
      return TRUE;
    return FALSE;
  }
  // </editor-fold>
  
  /**
   * Tries to establish a connection and do some query test   
   * 
   * @param Exception $error_callback If something go wrong, the exception will be stored in it
   * @param boolean $suppress_error_message If TRUE, no message is shown to user
   * @return boolean Whether the connection attempt was succeed or not
   */
  function tryConnection(&$error_callback = NULL, $suppress_error_message = FALSE) {    
    $strquery = $this->getSchema()->getTestQuery();
    try {
      Database::getConnection($this->getName())
          ->query($strquery);
      return TRUE;
    } 
    catch (PDOException $ex) {
      //TODO LOG error $ex
      if (!is_null($error_callback)) {
        $error_callback = $ex;
      }
      
      if (!$suppress_error_message) {
        drupal_set_message(t('An error ocurred while trying to establish a connection: !exception', 
          array('!exception' => $ex->getMessage())), 'error'); 
      }      
      return FALSE;
    }
  }

}

/**
 * A facade class which its only purpose is to test the connection in a easy-and-fast way
 * 
 * @author Guilherme
 */
class ConnectionTester {
  
  /**
   * Tries to perforrm a connection attempt with its informations
   * 
   * @param string $database The database name
   * @param string $host The database's host
   * @param string $user The username for the login attempt
   * @param string $pass The password for the login attempt
   * @param string $driver The database's driver (e.g. mysql, postgree)
   * @param Exception $error_callback If something go wrong, the exception will be stored in it
   * @param boolean $suppress_error_message If TRUE, no message is shown to user
   * 
   * @return boolean Whether the connection attempt was succeed or not
   */
  static function tryConnection($database, $host, $user, $pass, $driver, &$error_callback = NULL, $suppress_error_message = FALSE) {
    $obj = new ConnectionInfo('testing', $database, $host, $user, $pass, $driver, $error_callback, $suppress_error_message);
    
    return ConnectionTester::tryConnectionByObject($obj);
  }
  
  /**
   * Tries to perform a connection attempt using a Connection Info object
   * 
   * @param ConnectionInfo $coninfo The connection to perfom the test
   * @param Exception $error_callback If something go wrong, the exception will be stored in it
   * @param boolean $suppress_error_message If TRUE, no message is shown to user
   * 
   * @return boolean Whether the connection attempt was succeed or not
   */
  static function tryConnectionByObject($coninfo, &$error_callback = NULL, $suppress_error_message = FALSE) {    
    $original_name = $coninfo->getName();
    
    $coninfo->setName('testing');
    $coninfo->addToDrupal();
    $coninfo->setAsActiveConnection();
    
    $ret = $coninfo->tryConnection($error_callback, $suppress_error_message);
    
    $coninfo->setName($original_name);
    $coninfo->setActiveConnectionToDefault();
    $coninfo->removeFromDrupal();
    
    return $ret;    
  }
}

/**
 * A facade class to get an array with all available tables
 */
class ConnectionTableGetter {
  
/**
   * Gets all available tables from database
   * 
   * @param string $database The database name
   * @param string $host The database's host
   * @param string $user The username for the login attempt
   * @param string $pass The password for the login attempt
   * @param string $driver The database's driver (e.g. mysql, postgree)
   * 
   * @return array An associative array with all available tables
   */
  static function getTables($database, $host, $user, $pass, $driver) {
    $obj = new ConnectionInfo('testing', $database, $host, $user, $pass, $driver);
    
    return ConnectionTableGetter::getTablesByObject($obj);
  }
  
  /**
   * Gets all available tables from database
   * 
   * @param ConnectionInfo $coninfo
   * @return array An associative array with all available tables
   */
  static function getTablesByObject($coninfo) {
    $original_name = $coninfo->getName();
    
    $coninfo->setName('testing');
    $coninfo->addToDrupal();
    $coninfo->setAsActiveConnection();
        
    $tabs = $coninfo->getSchema()->getTables();
    
    $coninfo->setActiveConnectionToDefault();
    $coninfo->removeFromDrupal();
    $coninfo->setName($original_name);
    
    return $tabs;
  }
}