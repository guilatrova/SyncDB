<?php

/**
 * MySQL engine class implementation for the Sync DB Module
 *
 * @author Guilherme
 */
class SyncDB_MySQL extends SyncDB_Schema {
  /**
   * {@inheritdoc}
   */
  public function getTables() {
    $tables = array();
    $args = array(':database' => $this->getConnectionInfo()->getDatabase());
    $sql = 'SELECT table_name, table_comment
            FROM information_schema.tables
            WHERE table_schema=:database ';
        
    $res = db_query($sql, $args);
    foreach ($res as $r) {
      $tables[$r->table_name] = $r->table_comment;
    }    
    
    return $tables;
  }

  /**
   * {@inheritdoc}
   */
  public function getTestQuery() {
    return "SELECT table_name
              FROM information_schema.tables
              WHERE table_schema= '" . $this->parameters['database'] . "'";
  }

  /**
   * {@inheritdoc}
   */
  protected function getFieldTypeMap() {
    $map = array(
    'varchar:normal'  => 'VARCHAR',
    'char:normal'     => 'CHAR',

    'text:tiny'       => 'TINYTEXT',
    'text:small'      => 'TINYTEXT',
    'text:medium'     => 'MEDIUMTEXT',
    'text:big'        => 'LONGTEXT',
    'text:normal'     => 'TEXT',

    'serial:tiny'     => 'TINYINT',
    'serial:small'    => 'SMALLINT',
    'serial:medium'   => 'MEDIUMINT',
    'serial:big'      => 'BIGINT',
    'serial:normal'   => 'INT',

    'int:tiny'        => 'TINYINT',
    'int:small'       => 'SMALLINT',
    'int:medium'      => 'MEDIUMINT',
    'int:big'         => 'BIGINT',
    'int:normal'      => 'INT',

    'float:tiny'      => 'FLOAT',
    'float:small'     => 'FLOAT',
    'float:medium'    => 'FLOAT',
    'float:big'       => 'DOUBLE',
    'float:normal'    => 'FLOAT', 

    'numeric:normal'  => 'DECIMAL',

    'blob:big'        => 'LONGBLOB',
    'blob:normal'     => 'BLOB',
  );
  return array_flip($map);
}

  /**
   * {@inheritdoc}
   */
  public function getPrimaryKeys($tables) {
    $sql_and_tables = 'AND table_name IN (';
    foreach($tables as $t) {      
      $sql_and_tables .= "'$t',";      
    }
    $sql_and_tables = rtrim($sql_and_tables, ',') . ')';
    
    $database = Database::getConnectionInfo()['default']['database'];
    $sql = "SELECT table_name, column_name, index_name, sub_part, non_unique
              FROM information_schema.statistics
              WHERE table_schema='$database' $sql_and_tables and index_name ='PRIMARY' 
              ORDER BY table_name, index_name, seq_in_index";

    $res = db_query($sql);
    $primary = array();
    foreach ($res as $r) {
      $primary[$r->table_name][] = $r->column_name;
    }
    
    return $primary;
  }

  /**
   * {@inheritdoc}
   */
  public function getTableStructure($database, $chosen_tables, $supress_warnings = TRUE, $key = 'real_name') {
    $sql_and_tables = 'AND table_name IN (';
    foreach($chosen_tables as $t) {
      $name = $t[$key];
      $sql_and_tables .= "'$name',";      
    }
    $sql_and_tables = rtrim($sql_and_tables, ',') . ')';

    $sql = "SELECT table_name, column_type, column_name, column_default,
                   extra, is_nullable, numeric_scale, column_comment
            FROM information_schema.columns
            WHERE table_schema= '$database' ";    
    $sql .= $sql_and_tables;
    $sql .= ' ORDER BY table_name, ordinal_position';    
    
    $res = db_query($sql);
    $tables = array();
    foreach ($res as $r) {
      $numeric = !is_null($r->numeric_scale);
      $col = array();
      $col['type'] = $r->column_type;
      $col['name'] = $r->column_name;
      $col['table'] = $r->table_name;
      if (preg_match('@([a-z]+)(?:\((\d+)(?:,(\d+))?\))?\s*(unsigned)?@', $col['type'], $matches)) 
      {
        $col['type'] = $matches[1];
        if(isset($matches[2]))
          $col['size'] = $matches[2];        
        $col = $this->getFieldType($col, $supress_warnings);
        
        if (isset($matches[2])) 
        {          
          if ($col['type'] == 'numeric' || $col['type'] == 'float' || $col['type'] == 'double') {
            $col['precision'] = $matches[2];
            $col['scale'] = $matches[3];
          }
          elseif (!$numeric) {
            $col['length'] = $matches[2];
          }
        }
        if (isset($matches[4])) {
          $col['unsigned'] = TRUE;
        }
      }
      
      if ($col['type'] == 'int' && isset($r->extra) &&
          $r->extra == 'auto_increment') {
        $col['type'] = 'serial';
      }
      $col['not null'] = ($r->is_nullable == 'YES' ? FALSE : TRUE);
      if (! is_null($r->column_default)) {
        if ($numeric) {
          // XXX floats!
          $col['default'] = intval($r->column_default);
        }
        else {
          $col['default'] = $r->column_default;
        }
      }
      $col['description'] = $r->column_comment;
      $tables[$r->table_name]['fields'][$r->column_name] = $col;
    }

    $sql = "SELECT table_name, column_name, index_name, sub_part, non_unique
            FROM information_schema.statistics
            WHERE table_schema='$database' ";
    
    $sql .= $sql_and_tables;
    $sql .= 'ORDER BY table_name, index_name, seq_in_index';

    $res = db_query($sql);
    foreach ($res as $r) {
      if (isset($r->sub_part) && !is_null($r->sub_part)) {
        $col = array($r->column_name, intval($r->sub_part));
      }
      else {
        $col = $r->column_name;
      }
      if ($r->index_name == 'PRIMARY') {
        $type = 'primary key';
        $tables[$r->table_name][$type][] = $col;
        continue;
      }
      elseif ($r->non_unique == 0) {
        $type = 'unique keys';
      }
      else {
        $type = 'indexes';
      }
      $tables[$r->table_name][$type][$r->index_name][] = $col;
    }
    
    return $tables;
  }

}
