<?php

/*
 * @file
 * Handles all exceptions thrown by sync db module
 */

/**
 * Description of excps
 *
 * @author Guilherme
 */
class SyncDB_Exception extends Exception {
  function __construct($message, $code = 0, $previous = NULL) {
    parent::__construct($message, $code, $previous);
  }
}

class AttemptsExhaustedException extends SyncDB_Exception {
  
  private $atmpts;
  
  function __construct($message, $amt, $code = 0, $previous = NULL) {
    parent::__construct($message, $code, $previous);
    $this->atmpts = $amt;
  }
  
  /**
   * Gets the amount of attempts
   * 
   * @return int The amount of attempts performed
   */
  function getAmountAttempts() {
    return $this->atmpts;
  }
}

class NoDataException extends SyncDB_Exception {}

class InvalidCredentialsException extends SyncDB_Exception {
  private $user;
  private $database;
  
  function __construct($user, $database, $message = '', $code = 0, $previous = NULL) {
    $this->user = $user;
    $this->database = $database;
    
    parent::__construct($message, $code, $previous);
  }

  /**
   * Gets the username set to connect to database
   * 
   * @return string The login user
   */
  function getUser() {
    return $this->user;
  }

  /**
   * Gets the database name set to connect
   * 
   * @return string The database name
   */
  function getDatabase() {
    return $this->database;
  }
}

class ServerNotRespondException extends SyncDB_Exception {
  private $host;
  
  function __construct($host, $message = '', $code = 0, $previous = NULL) {
    $this->host = $host;
    parent::__construct($message, $code, $previous);
  }
  
  function getHost() {
    return $this->host;
  }
}

class StructureGapsException extends SyncDB_Exception {
  
  private $amtGaps;
  
  function __construct($message, $amt, $code = 0, $previous = NULL) {
    parent::__construct($message, $code, $previous);
    $this->amtGaps = $amt;
  }
  
  /**
   * Gets the amount of gaps
   * 
   * @return int The amount of gaps found
   */
  function getAmountGaps() {
    return $this->amtGaps;
  }
}
