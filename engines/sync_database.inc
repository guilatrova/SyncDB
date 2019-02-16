<?php

/**
 * A class that performs the synchronization between two databases
 *
 * @author Guilherme
 */
class SyncDatabase {
  const CONDITIONAL_QUERY = 0;
  const FULL_QUERY = 1;
  
  /**
   * @var ConnectionInfo
   */
  protected $connection;

  /**
   * @var SyncDB_Schema 
   */
  protected $schema;
  
  /**
   * @param SyncDB_Schema $schema
   */
  function __construct(SyncDB_Schema $schema) {
    $this->schema = $schema;
    $this->connection = $schema->getConnectionInfo();
  }
  
  // <editor-fold defaultstate="collapsed" desc="Get methods">
  
  /**
   * Gets a ConnectionInfo
   * @return ConnectionInfo The connection info being used by this instance
   */
  function getConnectionInfo() {
    return $this->connection;
  }
  
  /**
   * Gets a schema
   * @return SyncDB_Schema The schema being used by this instance
   */
  function getSchema() {
    return $this->schema;
  }

  /**
   * Gets the third-party data fetched as array from the tables passed as array
   * 
   * @param Array $tables An array with all third-party tables' names 
   * (in third-party database) to get data
   * @param int $type If the query is conditional or full
   * @param int $track_id (Optional) The ID to track progress
   * @return Array All data indexed by tables' name
   */
  function getData($tables, $type = SyncDatabase::CONDITIONAL_QUERY) {
    $this->connection->setAsActiveConnection();    
    $data = array();    
    foreach ($tables as $tabname) {
      
      try {
        $query =  db_select($tabname, 'generic')
          ->fields('generic');
      
        if ($type == SyncDatabase::CONDITIONAL_QUERY) {
          $query = $this->setCondition($query, $tabname);
        }

        $data[$tabname] = $query->execute()
            ->fetchAll(PDO::FETCH_ASSOC);
      }
      catch (PDOException $ex) {
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
            
            
          //case 42000: //No privileges to this table
          default: //Probably a syntax error, access violation or something like that
              drupal_set_message(t("Couldn't retrieve !table's data: !excp", array(
                '!table' => $tabname,
                '!excp' => $ex->getMessage(),
              )), 'error');
        }
      }
    }
    
    $this->connection->setActiveConnectionToDefault();
    return $data;
  }
  
  /**
   * Gets the primary key indexed fields to look for occurences in local database
   * @param Array $tables An array with all tables' names (in local database) 
   * to search by occurences
   */
  function getLocalOccurences($tables) {
    
    $keys = $this->getSchema()->getPrimaryKeys($tables);
    $occu = array();
    foreach ($tables as $tab) {
      $occu[$tab] = db_select($tab, 'tab')
        ->fields('tab', $keys[$tab])
        ->execute()
        ->fetchAll(PDO::FETCH_ASSOC);      
    }
    
    return $occu;
  }
  
  //</editor-fold>
  
  /**
   * Inserts or updates the data on local database
   * 
   * @param Array $data An array indexed by table's name (third-party database) with all data to sync
   * @param int $track_id (Optional) The ID to track the progress through the sync
   * @return Array A report with the status of the synchronization
   */
  function synchronize($data, $track_id = 0) {
    $this->setProgress($track_id, t('Getting ready to sync...'), 0);
    
    //Setting up the report
    $report_status['start_time'] = microtime(true);
    $report_status['records'] = 0;
    $report_status['updated'] = 0;
    $report_status['inserted'] = 0;
    $report_status['fails'] = 0;
    
    //Getting tables' name associations
    $tables_helper = $this->getConnectionInfo()->getTables(); //All tables from this connection
    $ttnames = array_keys($data); //Third-party Table Names
    $ltnames = array(); //Local Table Names
    foreach ($ttnames as $origname) {
      $ltnames[] = $this->getConnectionInfo()->getLocalTableName($origname);
    }
        
    //Getting occurences and primary keys in local database
    $occu = $this->getLocalOccurences($ltnames);    
    $keys = $this->getSchema()->getPrimaryKeys($ltnames);    

    //Counting records (general and individual)
    $syncount = 0; //Records sync
    foreach ($data as $thirdtab => $table_data) {
      $report_table[$thirdtab]['records'] = count($table_data);
      $report_status['records'] += $report_table[$thirdtab]['records'];
    }
    
    //Sync starts here
    foreach ($data as $thirdtab => $table_data) {
      $report_table[$thirdtab]['updated'] = 0;
      $report_table[$thirdtab]['inserted'] = 0;
      $report_table[$thirdtab]['fails'] = 0;
      
      $localtab = $tables_helper[$thirdtab]['local_name'];
      
      //foreach ($table_data as $row) {
      for ($row_count = 0; $row_count < count($table_data); $row_count++) {
        $row = $table_data[$row_count];
        
        if ($track_id) {//Don't waste time if user doesn't want to track
          $this->setProgress($track_id, t('Synchronizing !tabname (!cur of !amt)', array(
            '!tabname' => $localtab,
            '!cur' => $row_count+1,
            '!amt' => $report_table[$thirdtab]['records']
          )), ($syncount * 100 / $report_status['records']));
        }
        
        unset($row['@sync_db_exception']); //Index used to store exceptions
        try {
          if (empty($keys[$localtab]))
            drupal_set_message(t("Since !table hasn't any primary keys, all its data will be inserted", array(
              '!table' => $localtab
            )), 'warning');
          
          if ($this->isUpdate($row, $occu[$localtab], $keys[$localtab])) {
            $query = db_update($localtab)
                ->fields($row);

            foreach ($keys[$localtab] as $pkwhere) {
              $query = $query->condition($pkwhere, $row[$pkwhere]);
            }

            $query->execute();
            $report_status['updated']++;
            $report_table[$thirdtab]['updated']++;
          }
          else {
            db_insert($localtab)
              ->fields($row)
              ->execute();
            
            $report_status['inserted']++;
            $report_table[$thirdtab]['inserted']++;
          }
        }
        catch(Exception $ex) {
          $report_status['fails']++;
          $report_table[$thirdtab]['fails']++;          
          $row['@sync_db_exception'] = $ex->getMessage();
          $report_table[$thirdtab]['failed'][] = $row;
        }
        $syncount++;
        //http://drupal.stackexchange.com/questions/53318/how-can-i-save-arrays-of-values-in-the-database
      }
      
      if ($report_table[$thirdtab]['updated'] > 0 || $report_table[$thirdtab]['inserted'] > 0) {
        $tables_helper[$thirdtab]['last_sync'] = time();
        $rate = 100;
        if (isset($report_table[$thirdtab]['failed'])) {
          $rate = 100 - ((100 * count($report_table[$thirdtab]['failed'])) / 100);
        }
        
        $tables_helper[$thirdtab]['last_sync_success_rate'] = $rate;
      }
      else {
        drupal_set_message(t('Table !table was not synchronized', array(
          '!table' => $localtab,
        )), 'warning');
      }
    }
    
    $this->getConnectionInfo()->setLastSyncToNow();
    $this->getConnectionInfo()->setTables($tables_helper);
    $this->getConnectionInfo()->save();
    
    $report_status['end_time'] = microtime(true);
      
    $report['status'] = $report_status;
    $report['tables'] = $report_table;
    
    return $report;
  }
  
  /**
   * Sets the condition for the query and returns it
   * 
   * @param SelectQuery $query The query to set condition
   * @param string $table The (real) table name
   */
  protected function setCondition(SelectQuery $query, $table) {
    $tabinfo = $this->getConnectionInfo()->getTables()[$table];
    
    if ($cond = $tabinfo['condition']) {
      $clause = $cond;
      $args = array();
      $arg_count = 0;
      
      //Replacing condition info for where clause ➡TO. 023 and ➡TO. 022
      while (($pos = strpos($clause, '{')) !== FALSE) { //!== FALSE" required. Check it on: http://php.net/manual/pt_BR/function.strpos.php
        $beg = $pos+1;
        $end = strpos($clause, '}');
        if ($end) {//The end never can be 0          
          $arg = substr($clause, $beg, $end-$beg);
          if (substr($arg, 0, 1) == '@') { //Replace from info table?
            $arg = $tabinfo[substr($arg, 1, strlen($arg)-1)];
          }
          $beg--; $end++; //Avoiding '{' and '}'
          $plhold = ":arg$arg_count";
          $args[$plhold] = $arg;
          $clause = substr($clause, 0, $beg) .
              $plhold . substr($clause, $end, strlen($clause) - $end);
          $arg_count++;
        }
        else {
          //Error, condition is in invalid format
          return $query;
        }
      }
      
      $query->where($clause, $args);      
    }    
    return $query;
  }
  
  /**
   * Determines if a row should be updated or inserted
   * 
   * @param Array $search The array object that you want to determine if is a update or not
   * @param Array $arr The array that you want to check for occurences
   * @param Array $props The primary key fields to determine if it's an occurence
   * @return boolean Whether is an update
   */
  protected function isUpdate($search, $arr, $props) {
    $qtd_prop = count($props);
    if ($qtd_prop == 0)
      return FALSE;
    
    foreach($arr as $occ) {
      $qtd_aux = 0;
      foreach($props as $prop) {      
        if ($occ[$prop] == $search[$prop]) {
          $qtd_aux++;
        }
      }

      if ($qtd_aux == $qtd_prop)
        return TRUE;
    }

    return FALSE;
  }
  
  /**
   * Set the progress message and percentage to be able to track
   * 
   * @param int $track_id The ID to store progress
   * @param string $message The message to be displayed
   * @param int $perc A percentage value between 1 and 100
   */
  protected function setProgress($track_id, $message, $perc) {
    if ($track_id) {
      variable_set('sdb_prog_' . $track_id . '_mes', $message);
      variable_set('sdb_prog_' . $track_id, $perc);
    }
  }
}

/**
 * A facade class for SyncDatabase
 * @see SyncDatabase
 */
class Synchronization {
  
  /**
   * @var int 
   */
  private $attempts = 0;
  
  /**
   * @var Array 
   */
  private $failed_rows = array();
  
  /**
   * @var Array
   * @see on PrjArrayViewer, SyncDatabase::Synchronize 
   */
  private $report = FALSE;
  
  /**
   * @var SyncDatabase
   */
  private  $syncdb;
  
  /**
   * Creates and returns a Synchronization instance through a ConnectionInfo
   * @param ConnectionInfo $connection
   * @return \Synchronization
   */
  static function createFromConnection($connection) {
    return new Synchronization(new SyncDatabase($connection->getSchema()));
  }
  
  /**
   * Returns an array with the message and the percentage
   * 
   * @param int $track_id The ID of the progress which you want to track
   */
  static function getProgress($track_id) {
    
    $progress['percentage'] = intval(variable_get('sdb_prog_' . $track_id, 0));
    $progress['message'] = variable_get('sdb_prog_' . $track_id . '_mes', '');
    
    return $progress;
  }  
  
  /**
   * @param SyncDatabase $syncdb
   */
  function __construct(SyncDatabase $syncdb) {
    $this->syncdb = $syncdb;
  }

  /**
   * @return String The elapsed time in last synchronization
   */
  function getElapsedTime() {
    return format_date($this->elapsed_time);
  }

  /**
   * @return Array The generate report in last synchronization
   */
  function getReport() {
    return $this->report;
  }    

  /**
   * Gets the delay time to wait between synchronizations
   * 
   * @return int The amount of seconds to wait until the next attempt
   */
  function getDelay() {
    return variable_get('sync_db_interval_sync', 5) * 60;
  }
  
  /**
   * Evals if the amount of attempts reached its limit
   * 
   * @return boolean Whether the attempts were exhausted
   */
  function attemptsExhausted() {
    return ($this->attempts >= variable_get('sync_db_attempts_amt_sync', 3));
  }
  
  /**
   * Perfoms a synchronization of some tables
   * @param Array $tables An array with all third-party tables' names
   * @param boolean $verify Whether the module should perform a validation on structure
   * before synchronizing
   * 
   * @return boolean Whether the synchronization was successful or not
   */
  function synchronize($tables, $options = array()) {
    //A value of 0 indicates unlimited execution time: https://api.drupal.org/api/drupal/includes!common.inc/function/drupal_set_time_limit/7
    drupal_set_time_limit(0);
    //Setting up config
    $track = isset($options['track']) ? $options['track'] : 0;
    $verify = isset($options['verify']) ? $options['verify'] : 0;
    $type = isset($options['type']) ? $options['type'] : SyncDatabase::CONDITIONAL_QUERY;
    
    $this->report = FALSE;
    $this->attempts = 0;
    $this->setProgressMessage($track, t('Getting start...'));
    
    do {
      $this->attempts++;
      $failed = FALSE;
      
      try {
        $this->verify($verify, $track); //Verify structure gaps and repair (both only if set in options)
        $data = $this->getData($tables, $type, $track); //Retrieves data from third-party database
        $report = $this->syncdb->synchronize($data, $track); //Performs sync
        $this->populateFromReport($report); //Store report info
      }
      catch (InvalidCredentialsException $ex) {
        $this->delVariables($track);
        throw new SyncDB_Exception(t('Synchronization aborted due Invalid Credentials.'), $ex->getCode(), $ex);
      }
      catch (StructureGapsException $ex) {
        $this->delVariables($track);
        throw new StructureGapsException(t('Synchronization aborted due structure gaps found.'), $ex->getAmountGaps(), $ex->getCode(), $ex);
      }
      catch (SyncDB_Exception $ex) {
        $failed = TRUE;
                
        if ($this->attemptsExhausted())
          throw new AttemptsExhaustedException(t('Max attempts attained.'), $this->attempts, $ex->getCode(), $ex);
        
        if ($track) {
          $messg = t('Attempt[!atmpt] failed: !excp. Trying again in !time.', array(
            '!atmpt' => $this->attempts,
            '!excp' => $ex->getMessage(),
          ));
          $end = time() + $this->getDelay();
          while (time() < $end) {
            $this->setProgressMessage($track, str_replace('!time', gmdate("i:s", $end - time()), $messg));
            sleep(0.9);
          }
        }
        else {
          sleep($this->getDelay());
        }
      }
      
    } while($failed && $this->canRetry());
    
    $this->delVariables($track);
    return ($this->report['status']['fails'] === 0);
  }
  
  /**
   * Performs verification and repair if set (and needed)
   * 
   * @param int $verify 0 for no verification, 1 for just verification and 2 for verification and repair
   * @param int $track The track ID
   */
  private function verify($verify, $track) {
    if ($verify) {
      $this->setProgressMessage($track, t("Verifying tables' structure"));
      $repair = RepairSyncDB::createFromConnection($this->syncdb->getConnectionInfo());

      if (!$repair->verify(TRUE)) { //Check for structure gaps
        if ($verify == 2) {
          $this->setProgressMessage($track, t('Fixing tables mismatches...'));
          $repair->repair();
        }
        else {
          $mmt = count($repair->getReport()['diff']);
          throw new StructureGapsException(t('Structure gaps found'), $mmt);
        }
      }
    }
  }
  
  /**
   * Returns the third-party data
   * 
   * @param array $tables An array with all tables' (third-party) name
   * @param int $type Conditional or Full query
   * @param int $track The track ID
   * @return Array Retrieved data indexed by tables' (third-party) name
   */
  private function getData($tables, $type, $track) {
    $this->setProgressMessage($track, t("Retrieving tables' data"));
    $data = $this->syncdb->getData($tables, $type);
    
    if (!array_filter($data))
      throw new NoDataException(t("Cannot retrieve any table data or there's nothing to sync"));
    
    return $data;
  }
  
  /**
   * Synchronize just the failed rows on last synchronization
   * @return boolean Whether the synchronization was successful or not
   */
  function synchronizeFailedRows() {
    if ($this->report['status']['fails'] > 0) {

      $report = $this->syncdb->synchronize($this->failed_rows);
      $this->populateFromReport($report);
    }
    
    return ($this->report['status']['fails'] == 0);
  }
  
  /**
   * Evals the amount of attempts
   * 
   * @return boolean Whether the module can retry a sync
   */
  private function canRetry() {
    return ($this->attempts < variable_get('sync_db_attempts_amt_sync', 3));
  }
  
  /**
   * Populates fields from last sync report's result
   */
  private function populateFromReport($report) {
    $this->report = $report;
    $this->elapsed_time = $report['status']['end_time'] - $report['status']['start_time'];
    
    $retry = array();
    foreach ($report['tables'] as $tabname => $info) {
      if (isset($info['failed']))
        $retry[$tabname] = $info['failed'];
    }
    $this->failed_rows = $retry;
  }
  
  /**
   * Sets a message to be displayed in the progress bar
   * 
   * @param int $track_id The ID to track the progress
   * @param string $message The message to display
   */
  private function setProgressMessage($track_id, $message) {
    if ($track_id)
      variable_set('sdb_prog_' . $track_id . '_mes', $message);
  }
  
  /**
   * Deletes the variables for that sync progress
   * 
   * @param int $track_id The ID to delete
   */
  private function delVariables($track_id) {
    if ($track_id) {
      variable_del('sdb_prog_' . $track_id . '_mes');
      variable_del('sdb_prog_' . $track_id);    
    }
  }
}

class ScheduledSync {
  /**
   * The tolerance time in minutes to sync
   */
  const TOLERANCE = 30;
  
  /**
  * @var int 
  */
  private $id;
  
  /**
   * @var int
   */
  private $cid;
  
  /**
   * @var Array 
   */
  private $tables;
  
  /**
   * @var Array 
   */
  private $unsaved_tables;
  
  /**
   * @var String
   */
  private $description;
  
  /**
   * Crontab format (➡TO. 026)
   *# +---------------- minute (0 - 59)
   *# |  +------------- hour (0 - 23)
   *# |  |  +---------- day of month (1 - 31)
   *# |  |  |  +------- month (1 - 12)
   *# |  |  |  |  +---- day of week (0 - 6) (Sunday=0)
   *# |  |  |  |  |
   *# *  *  *  *  *  command to be executed
   * @var String
   * @see https://www.drupal.org/node/23714
   */
  private $frequency;

  /**
   * Unix timestamp
   * @var int 
   */
  private $next_sync;
  
  /**   
   * @var ConnectionInfo 
   */
  protected $connection;
  
  /**
   * @var String
   */
  protected $options;
  
  /**
   * Creates the schedule for the connection
   * @param ConnectionInfo $con
   * @return \ScheduledSync
   */
  static function createSchedule(ConnectionInfo $con) {
    $ss = new ScheduledSync();
    $ss->connection = $con;
    $ss->cid = $con->getId();
    
    return $ss;
  }
  
  private function __construct()
  {
    $ss->description = "";
    $ss->frequency = "4 0 * * 0";
    $ss->id = 0;
    $ss->next_sync = -1;   
  }
  
  // <editor-fold defaultstate="collapsed" desc="Get Methods">
  /**
   * Gets the ID
   * @return int
   */
  function getId() {
    return $this->id;
  }

  /**
   * Gets the description
   * @return String
   */
  function getDescription() {
    return $this->description;
  }

  /**
   * Gets the next sync scheduled
   * @param boolean $unix_timestamp Wheather use Unixtimestamp format
   * @return String|int The next sync
   */
  function getNextSync($unix_timestamp = FALSE) {
    if ($unix_timestamp)      
      return $this->next_sync;
    return date('Y-m-d h:i:s',$this->next_sync);
  }

  /**
   * Gets the connection which will be sync
   * @return ConnectionInfo
   */
  function getConnectionInfo() {
    if ($this->connection == NULL)
      $this->connection = ConnectionInfo::getConnectionInfoById($this->cid);
    return $this->connection;
  }

  /**
   * Gets the options set
   * @return Array The options
   */
  function getOptions() {
    $exp = explode(';',$this->options);
    
    $ret['verify'] = $exp[0];
    $ret['type'] = $exp[1];
    
    return $ret;
  }
  
  /**
   * Gets the sync frequency 
   * @return String
   */
  function getSchedule($crontab = FALSE) {
    if ($crontab)
      return $this->frequency;
    
    $strsc = '';
    $sch = explode(' ', $this->frequency);
    return $strsc;
  }
  
  /**
   * Gets the tables
   * @return Array An array with all the tables to be sync
   */
  function getTables() {
    return $this->tables;
  }
  // </editor-fold>

  // <editor-fold defaultstate="collapsed" desc="Set Methods">
  
  /**
   * Sets the schedule's description
   * @param String $description The description to be set
   */
  function setDescription($description) {
    $this->description = $description;
  }

  /**
   * Sets the frequency for the connection to be sync
   * @param int $frequency
   */
  function setSchedule($frequency) {
    $this->frequency = $frequency;
  }

  /**
   * Sets the Synchronization options 
   * @param Array $options An array with the options (verify and type)
   */
  function setOptions($options) {
    $stropt = (isset($options['verify'])) ?
        $options['verify'] : '0';
    
    //➡TO. 025
    $stropt .= ';';
    $stropt .= (isset($options['type'])) ?
        $options['type'] : SyncDatabase::CONDITIONAL_QUERY;
    
    $this->options = $stropt;
  }
  
  /**
   * Sets the tables to be sync
   * @param array $tables The tables to be sync
   */
  function setTables(Array $tables) {
    $this->unsaved_tables = $tables;
  }
  // </editor-fold>
    
  // <editor-fold defaultstate="collapsed" desc="Database">
  /**
   * Inserts or updates the current object in database
   */
  function save() {
    $this->calcNextSync();
    
    if ($this->id > 0) { //Update
      db_update('scheduled_sync', 'ss')
          ->fields('ss', array(
            'options' => $this->options,
            'description' => $this->getDescription(),
            'frequency' => $this->getSchedule(),
            'next_sync' => $this->next_sync
          ))
          ->condition('ssid', $this->id)
          ->execute();
      
      //Looking tables to stop syncing
      foreach ($this->tables as $tabname => $tab) {
        if (!isset($this->unsaved_tables[$tabname])) {//Do not sync this anymore
          db_delete('scheduled_sync_tables', 'sst')
              ->condition('ssid', $this->id)
              ->condition('tid', $tab['tid'])
              ->execute();
        }
      }
      
      //Looking for new tables to sync
      foreach ($this->unsaved_tables as $tabname => $tab) {
        if (!isset($this->tables[$tabname])) { //Sync this from now on
          db_insert('scheduled_sync_tables', 'sst')
              ->fields('sst', array(
                'ssid' => $this->id,
                'tid' => $tab['tid']
              ))
              ->execute();
        }
      }
    }
    else { //Insert
      $id = db_insert('scheduled_sync', 'ss')
          ->fields('ss', array(
            'options' => $this->options,
            'description' => $this->getDescription(),
            'frequency' => $this->frequency,
            'next_sync' => $this->next_sync
          ))
          ->execute();
      
      $this->id = $id;
      foreach ($this->unsaved_tables as $tab) {
        db_insert('scheduled_sync_tables', 'sst')
            ->fields('sst', array(
              'ssid' => $id,
              'tid' => $tab['tid']
            ))
            ->execute();
      }
    }
    
    $this->tables = $this->unsaved_tables;
  }
  
  /**
   * Deletes from database
   */
  function delete() {    
    if ($this->id > 0) {
      db_delete('scheduled_sync_tables')
        ->condition(array('ssid', $this->id))
        ->execute();
      
      db_delete('scheduled_sync')
        ->condition(array('ssid', $this->id))
        ->execute();
    }
  }
  
  /**
   * Gets all the scheduled syncs
   * @param ConnectionInfo $connection (Optional) Only the schedules for that connection
   * @return Array \ScheduledSync An array with all the scheduled syncs
   */
  static function getSchedules(ConnectionInfo $connection = NULL) {
    $schs = array();
    
    $query = db_select('scheduled_sync', 'ss')
        ->fields('ss');
    
    if ($connection != NULL) {
      $query = $query
          ->condition('cid', $connection->getId());
    }
    $rows = $query->execute();
    
    foreach ($rows as $row) {
      $obj = new ScheduledSync();
      $obj->cid = $row['cid'];
      $obj->options = $row['options'];
      $obj->description = $row['description'];
      $obj->frequency = $row['frequency'];
      $obj->next_sync = $row['next_sync'];
      
      $schs[] = $obj;
    }
    
    return $schs;
  }
  // </editor-fold>
  
  /**
   * Runs the synchronization if it is the right time
   */
  function run() {
    
  }
  
  /**
   * Calculates the next sync time based on the scheduled one
   */
  private function calcNextSync() {
    
  }
}
