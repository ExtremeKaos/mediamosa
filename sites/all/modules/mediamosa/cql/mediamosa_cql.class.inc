<?php
/**
 * @file
 * Main CQL class
 */

/**
 * cql               ::= cqlQuery ['sortby' sortSpec]
 * cqlQuery          ::= prefixAssignment cqlQuery | scopedClause
 * prefixAssignment  ::= '>' prefix '=' uri | '>' uri
 * scopedClause       ::= scopedClause booleanGroup searchClause | searchClause
 * booleanGroup       ::= boolean [modifierList]
 * boolean           ::= 'and' | 'or' | 'not' | 'prox'
 * searchClause      ::= '(' cqlQuery ')' | index relation searchTerm | searchTerm
 * relation          ::= comparitor [modifierList]
 * comparitor        ::= comparitorSymbol | namedComparitor
 * comparitorSymbol  ::= '=' | '>' | '<' | '>=' | '<=' | '<>' | '=='
 * namedComparitor   ::= identifier
 * modifierList      ::= modifierList modifier | modifier
 * modifier          ::= '/' modifierName [comparitorSymbol modifierValue]
 * sortSpec           ::= sortSpec singleSpec | singleSpec
 * singleSpec         ::= index [modifierList]
 * prefix, uri
 * modifierName,
 * modifierValue,
 * searchTerm, index ::= term
 * term              ::= identifier | 'and' | 'or' | 'not' | 'prox' | 'sortby'
 * identifier        ::= charString1 | charString2
 * charString1        := Any sequence of characters that does not include any of the following:
 *                       whitespace
 *                       ( (open parenthesis )
 *                       ) (close parenthesis)
 *                       =
 *                       <
 *                       >
 *                       '"' (double quote)
 *                       /
 * If the final sequence is a reserved word, that token is returned instead. Note that '.' (period)
 * may be included, and a sequence of digits is also permitted. Reserved words are 'and', 'or',
 * 'not', and 'prox' (case insensitive). When a reserved word is used in a search term,
 * case is preserved.
 *
 * charString2        := Double quotes enclosing a sequence of any characters except double quote
 *                       (unless preceded by backslash (\)). Backslash escapes the character
 *                       following it. The resultant value includes all backslash characters except
 *                       those releasing a double quote (this allows other systems to interpret the
 *                       backslash character). The surrounding double quotes are not included.
 *
 *
 *
 * [[index] relation[/modifier]] [term] [sortBy index[/modifier]]
 */

/**
 * Key defines for the context array
 */
define('MEDIAMOSA_CQL_CONTEXT_KEY_INDEXES', 'indexes');// CQL index
define('MEDIAMOSA_CQL_CONTEXT_KEY_COLUMNS', 'columns');// SQL column
define('MEDIAMOSA_CQL_CONTEXT_KEY_INDEX_REFERENCES', 'index_references'); // List of index => index, multiple indexes can be references
define('MEDIAMOSA_CQL_CONTEXT_KEY_DEFAULT_RELATION', 'default_relation');// required
define('MEDIAMOSA_CQL_CONTEXT_KEY_DEFAULT_SEARCH_TERMS', 'default_search_terms');// required

define('MEDIAMOSA_CQL_BOOLEAN_OR', 'OR');
define('MEDIAMOSA_CQL_BOOLEAN_AND', 'AND');
define('MEDIAMOSA_CQL_BOOLEAN_NOT', 'NOT');
define('MEDIAMOSA_CQL_BOOLEAN_PROX', 'PROX');

/**
 * Settings and defines
 */
define('MEDIAMOSA_CQL_KEY_SETTING_VERIFY', 'cql_key_setting_verify');

/**
 * CQL Class
 *
 * Contains the basic function for parsing the CQL string
 * Use cql_context instead
 */
abstract class mediamosa_cql {

  // ------------------------------------------------------------------- Consts.
  const RESPECTCASE = 'respectcase';
  const IGNORECASE = 'ignorecase';

  // Reserved CQL.
  const STEM = 'stem';
  const RELEVANT = 'relevant';
  const FUZZY = 'fuzzy';
  const PARTIAL = 'partial';

  // Mediamosa specific.
  const BOOST = 'boost';
  const KEEPORDER = 'keeporder';

  // --------------------------------------------------------------------- Vars.
  private $m_str_cql = FALSE;
  private $m_o_cql_part_group = FALSE;
  private $m_o_cql_sortby = FALSE;

  /**
   * The forced order set by CQL modifier keeporder.
   *
   * Only works on primary IDs, e.g. asset_id.
   *
   * Example: asset_ids any/keeporder "^1234^ ^1235^".
   *
   * @var array
   */
  private $keep_order_ids = array();

  // ------------------------------------------------------------------ Methods.
  /**
   * Set the keep order IDs for later sorting.
   *
   * Only works on primary IDs, e.g. asset_id.
   *
   * Example: asset_ids any/keeporder "^1234^ ^1235^".
   *
   * @param array $ids
   *   The IDs to set.
   */
  protected function mediamosa_core_set_keep_order($column_name, $ids) {
    $this->mediamosa_core_allowed_keep_order($column_name);
    $this->keep_order_ids = array_merge($this->keep_order_ids, $ids);
  }

  /**
   * Return the keep order IDs for later sorting.
   *
   * @return array
   *   The forced order.
   */
  public function mediamosa_core_get_keep_order() {
    return $this->keep_order_ids;
  }

  /**
   * Check if keeporder modifier is allowed.
   *
   * By default this will end in exception; Overrule in parent class.
   *
   * @param string $column_name
   *   The name to check.
   */
  protected function mediamosa_core_allowed_keep_order($column_name) {
    throw new mediamosa_cql_exception(mediamosa::t("The '@column_name' is not allowed to use modifier 'keeporder'.", array('@column_name' => $column_name)));
  }

  /**
   * Parse the CQL string.
   *
   * Will throw exceptions when errors are found in syntax.
   *
   * @param string $str_cql
   *   The CQL string to parse.
   * @param array $settings
   *   The settings for parse phase.
   */
  protected function mediamosa_cql_parse($str_cql, $settings = array()) {
    assert(is_string($str_cql));
    assert(is_array($settings));

    // Copy it for later usage
    $this->m_str_cql = (string )$str_cql;

    // Clear our collection of possible previous parse
    $this->m_o_cql_part_group = new mediamosa_cql_part_group();

    // Make sure these settings are set
    if (!isset($settings[MEDIAMOSA_CQL_KEY_SETTING_VERIFY])) {
      $settings[MEDIAMOSA_CQL_KEY_SETTING_VERIFY] = FALSE;
    }

    if ($str_cql === '' || $str_cql === FALSE || $str_cql === NULL) {
      throw new mediamosa_cql_exception_string_is_empty();
    }

    $i_pos = 0;

    try {
      while (1) {
        $this->mediamosa_cql_next_part((string) $str_cql, $i_pos, $this->m_o_cql_part_group);
      }
    }
    catch (mediamosa_cql_exception_eos $e) {
      // no error when we catch this here
    }
    catch (mediamosa_cql_exception_eo_scope $e) {
      assert($e);
      throw new mediamosa_cql_exception_eo_scope_unmatched();
    }
  }

  /**
   * Create new relation object.
   *
   * @param string $str_relation
   * @return object
   */
  protected function mediamosa_cql_new_part_relation_obj($str_relation) {
    return new mediamosa_cql_part_relation($str_relation);
  }

  /**
   * Create new index object.
   *
   * @param string $str_index
   * @return object
   */
  protected function mediamosa_cql_new_part_index_obj($str_index) {
    return new mediamosa_cql_part_index($str_index);
  }

  /**
   * Returns the main group object
   *
   * @return o_cql_part_group
   */
  protected function mediamosa_cql_part_group_get() {
    return $this->m_o_cql_part_group;
  }

  /**
   * Returns the sortby group
   *
   * @return o_cql_sortby
   */
  protected function mediamosa_cql_sortby_get() {
    return $this->m_o_cql_sortby;
  }

  /**
   * Verify the CQL string
   * Will throw exceptions when parsing failed
   *
   * @param string $str_cql
   * @return bool
   */
  protected function mediamosa_cql_verify($str_cql) {
    mediamosa_cql::mediamosa_cql_parse($str_cql, array(MEDIAMOSA_CQL_KEY_SETTING_VERIFY => TRUE));
  }

  /**
   * Next position in the string..
   * throws cql_exception_eos when it reached the end
   *
   * @param string $str_cql
   * @param int $i_pos
   * @return bool true; char is escaped
   */
  private function mediamosa_cql_next_pos($str_cql, &$i_pos, $is_quoted) {

    $i_pos++;

    if (mediamosa_unicode::strlen($str_cql) <= $i_pos) {
      throw new mediamosa_cql_exception_eos(); // end of string
    }

    // If next char is escape '\', then skip it and test if the next char is allowed to escaped
    if (mediamosa_unicode::substr($str_cql, $i_pos, 1) == '\\') {

      if (!$is_quoted) {
        throw new mediamosa_cql_exception_syntax_error('character \ can only be used inside "quoted" strings', $i_pos);
      }

      $i_pos++;

      if (mediamosa_unicode::strlen($str_cql) <= $i_pos) {
        throw new mediamosa_cql_exception_unexpected_eos(); // end of string
      }

      // Removed code to prevent escape all chars, its now allowed on all chars.

      return TRUE;
    }

    return FALSE;
  }

  /**
   * Skip Whitespaces
   *
   * @param string $str_cql
   * @param int $i_pos
   * @return integer
   */
  private function mediamosa_cql_skip_ws($str_cql, &$i_pos) {
    if (mediamosa_unicode::strlen($str_cql) <= $i_pos) {
      throw new mediamosa_cql_exception_eos(); // end of string
    }

    $is_quoted = FALSE;
    while (mediamosa_unicode::substr($str_cql, $i_pos, 1) == ' ') {
      $is_quoted = $this->mediamosa_cql_next_pos($str_cql, $i_pos, $is_quoted);
    }

    return $is_quoted;
  }

  /**
   * Check if anything is left in the string
   * will throw if end is reached, so only call
   * if you really need something to process or
   * expect it.
   *
   * @param string $str_cql
   * @param int $i_pos
   */
  private function mediamosa_cql_has_next($str_cql, $i_pos) {
    return $this->mediamosa_cql_skip_ws($str_cql, $i_pos);
  }

  /**
   * Get next cql (sub)part
   *
   * @param string $str_cql
   * @param int $i_pos
   * @param bool $is_quoted
   * @param bol $is_escaped
   * @param array $array_end_chars
   * @param array $array_end_words
   *
   * @return string
   *
   * @throws mediamosa_cql_exception_eos
   */
  private function mediamosa_cql_next($str_cql, &$i_pos, $is_quoted = FALSE, $is_escaped = FALSE, array $array_end_chars = array(' ', '/', '(', ')', '"'), array $array_end_words = array('<>', '<=', '>=', '==', '=', '>', '<')) {
    $str_result = '';

    $a_symbol_end_keys = array('<', '>', '=');

    try {
      // 1st skip any whitechars before we capture
      if (!$is_quoted) {
        $this->mediamosa_cql_skip_ws($str_cql, $i_pos);
      }

      // Get 1st char
      $str_result = ($is_escaped ? '\\' : '') . mediamosa_unicode::substr($str_cql, $i_pos, 1);

      if ($str_result == ')') {
        return $str_result;
      }

      // If it starts with '"', then we must end with one
      if ($str_result == '"') {
        if ($is_quoted) {
          return '';
        }

        return $this->mediamosa_cql_next_quoted($str_cql, $i_pos, $is_escaped);
      }

      // If it starts with '(', then we are opening a scope.
      // If it starts with ')', then we are closing a scope.
      if (!$is_quoted && $str_result == '(') {
        $i_pos++;
        return $str_result;
      }

      // Next char
      $is_escaped = $this->mediamosa_cql_next_pos($str_cql, $i_pos, $is_quoted);

      // If a '/', we can expect whitespaces, so skip them
      if (!$is_quoted && $str_result == '/') {
        $this->mediamosa_cql_skip_ws($str_cql, $i_pos);
      }

      $b_include_end_keys_alhpa = FALSE;
      if (in_array($str_result, $a_symbol_end_keys)) {
        $b_include_end_keys_alhpa = TRUE;
      }

      // Now get the next part until any of the end chars or eos
      while (1) {
        if (!$is_escaped) {
          // End chars will contain '"', when we are inside quoted
          if (array_search(mediamosa_unicode::substr($str_cql, $i_pos, 1), $array_end_chars) !== FALSE) {
            break;
          }

          if (!$is_quoted) {
            if ($b_include_end_keys_alhpa) {

              if (ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) >= ord('A') && ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) <= ord('Z')) {
                break;
              }

              if (ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) >= ord('a') && ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) <= ord('z')) {
                break;
              }

              if (ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) >= ord('0') && ord(mediamosa_unicode::substr($str_cql, $i_pos, 1)) <= ord('9')) {
                break;
              }
            }
            elseif (array_search(mediamosa_unicode::substr($str_cql, $i_pos, 1), $a_symbol_end_keys) !== FALSE) {
              break;
            }
          }
        }

        if (!$is_quoted) {
          foreach ($array_end_words as $end_word) {
            if (mediamosa_unicode::strlen($str_cql) >= mediamosa_unicode::strlen($end_word) + $i_pos) {
              if (mediamosa_unicode::substr($str_cql, 0, mediamosa_unicode::strlen($end_word)) == $end_word) {
                break;
              }
            }
          }
        }

        // Copy next char
        $str_result .= ($is_escaped ? '\\' : '') . mediamosa_unicode::substr($str_cql, $i_pos, 1);

        // Next position...
        $is_escaped = $this->mediamosa_cql_next_pos($str_cql, $i_pos, $is_quoted);
      }
    }
    catch (mediamosa_cql_exception_eos $e) {
      // No problem if we catch this
      if ($str_result === '' || $str_result === '/') {
        throw $e;
      }

      assert($str_result !== '"');
    }

    return $is_quoted ? stripslashes($str_result) : $str_result;
  }

  /**
   * Same as mediamosa_cql_next, except we are inside quoted string
   *
   * @param string $str_cql
   * @param int $i_pos
   * @return string
   */
  private function mediamosa_cql_next_quoted($str_cql, &$pos) {
    $is_escaped = $this->mediamosa_cql_next_pos($str_cql, $pos, TRUE);

    $str = '"' . $this->mediamosa_cql_next($str_cql, $pos, TRUE, $is_escaped, array('"')) . '"';

    // If the string didn't end with an ", the cql is not valid.
    if (mediamosa_unicode::strlen($str_cql) <= $pos || mediamosa_unicode::substr($str_cql, $pos, 1) != '"') {
      throw new mediamosa_cql_exception_eo_quote_unmatched();
    }

    $pos++;
    return $str;
  }

  /**
   * Trim " from $str
   *
   * @param string $str
   * @return string
   */
  private function mediamosa_cql_trim_quotes($str) {
    if (mediamosa_unicode::strlen($str) < 2) {
      return $str;
    }

    if (mediamosa_unicode::substr($str, 0, 1) == '"' && mediamosa_unicode::substr($str, -1, -1) == '"') {
      return mediamosa_unicode::substr($str, 1, -1);
    }

    return $str;
  }

  /**
   * Get the next complete part
   *
   * @param string $str_cql
   * @param int $i_pos
   */
  private function mediamosa_cql_next_part($str_cql, &$i_pos, &$o_cql_part_group) {

    // Get the first item from current pos.
    $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);

    // Create a new cql part object.
    $o_cql_part = new mediamosa_cql_part($o_cql_part_group, $this->mediamosa_cql_context_prefix_assignment_get());

    try {
      // Prefix assignment?
      while (mediamosa_unicode::substr($str_1, 0, 1) === '>') {

        // Get the next part.
        $str_2 = $this->mediamosa_cql_next($str_cql, $i_pos);

        // Check if the next char is a '='.
        if ($this->mediamosa_cql_next_is($str_cql, $i_pos, '=')) {
          // Get the next part.
          $str_3 = $this->mediamosa_cql_next($str_cql, $i_pos);

          if ($str_3 != '=') {
            throw new mediamosa_cql_exception_expecting_char('=', $i_pos);
          }

          // Get the next part.
          $str_4 = $this->mediamosa_cql_next($str_cql, $i_pos);

          $o_prefix_assignment = new mediamosa_cql_prefix_assignment($str_2, $this->mediamosa_cql_trim_quotes($str_4));
          $o_cql_part->mediamosa_cql_part_prefix_assignment_add($o_prefix_assignment);
        }
        else {
          $o_prefix_assignment = new mediamosa_cql_prefix_assignment('', $this->mediamosa_cql_trim_quotes($str_2));
          $o_cql_part->mediamosa_cql_part_prefix_assignment_add($o_prefix_assignment);
        }

        $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);
      }

      // May start with the word 'NOT'
      if ($this->mediamosa_cql_context_is_boolean_not($str_1)) {
        $o_cql_part->mediamosa_cql_part_has_boolean_not_set(TRUE);
        $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);
      }

      // '(' cqlQuery ')' | index relation searchTerm | searchTerm
      if ($str_1 == '(') {

        try {
          while (1) {
            $this->mediamosa_cql_next_part($str_cql, $i_pos, $o_cql_part);
          }
        }
        catch (mediamosa_cql_exception_eo_scope $e) {
          // Matching with this one, ignore
          // Expecting now boolean, sortby or eos
        }
      }
      elseif ($str_1 == ')') {
        $i_pos++;
        throw new mediamosa_cql_exception_eo_scope();
      }
      elseif (mediamosa_unicode::strtolower($str_1) != 'sortby')  {
        // str_1 possible index or searchTerm. We know when next is an relation or not

        // Check if this char_string_1 or _2 (both cases must be charString valid)
        $this->mediamosa_cql_must_be_charstring($str_1, $i_pos);

        // In case its a index...
        $o_cql_part_index = $this->mediamosa_cql_new_part_index_obj($str_1);
        assert($o_cql_part_index);

        $this->mediamosa_cql_modifiers_do($str_cql, $i_pos, $o_cql_part_index);

        // Get the next part
        $i_pos_old = $i_pos; // Incase its a single search term.

        try {
          $str_2 = $this->mediamosa_cql_next($str_cql, $i_pos);
        }
        catch (mediamosa_cql_exception_eos $e) {
          $str_2 = '';
        }

        // Next is either relation or search term
        if ($this->mediamosa_cql_context_is_relation($str_2)) {

          // If this an relation, then check if we created the index part

          // Add it to the main part.
          $o_cql_part->mediamosa_cql_part_index_set($o_cql_part_index);

          // Check the index.
          $o_cql_part->mediamosa_cql_part_verify_index($this, $i_pos - mediamosa_unicode::strlen($str_2));

          // Create relation part object.
          $o_cql_part_relation = $this->mediamosa_cql_new_part_relation_obj($str_2);

          // Do modifiers (if any).
          $this->mediamosa_cql_modifiers_do($str_cql, $i_pos, $o_cql_part_relation);

          // Add it to our object.
          $o_cql_part->mediamosa_cql_part_relation_set($o_cql_part_relation);

          // Next is search term.
          $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);

          // Check if this char_string_1 or _2.
          $this->mediamosa_cql_must_be_charstring($str_1, $i_pos);
        }
        else {
          // At this point $str_1 must be a single search term, so modifiers on this one is not allowed
          // On the optional index object, there should be no modifiers.
          if ($o_cql_part_index->mediamosa_cql_modifier_has()) {
            throw new mediamosa_cql_exception_syntax_error('modifiers not allowed here', $i_pos);
          }

          // Ok one single search term, change back our pos.
          $i_pos = $i_pos_old;
        }

        // Search term (required)
        $o_cql_part->mediamosa_cql_part_search_term_set(new mediamosa_cql_part_search_term($str_1));
      }

      if (mediamosa_unicode::strtolower($str_1) != 'sortby') {
        $i_pos_old = $i_pos;
        try {
          $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);
        }
        catch (mediamosa_cql_exception_eos $e) {
          // we are done, nothing left and we may end here too.
          $o_cql_part_group->mediamosa_cql_part_add($o_cql_part);
          return;
        }
      }

      if (mediamosa_unicode::strtolower($str_1) == 'sortby') { // Sortby
        $this->m_o_cql_sortby = new mediamosa_cql_sortby();

        // Get the next part
        $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);

        // Check if this char_string_1 or _2
        $this->mediamosa_cql_must_be_charstring($str_1, $i_pos);

        // Add index for sortby object
        $o_cql_sub_sortby = new mediamosa_cql_sub_sortby($str_1);

        // do modifiers (if any)
        $this->mediamosa_cql_modifiers_do($str_cql, $i_pos, $o_cql_sub_sortby);

        // Ok add it
        $this->m_o_cql_sortby->mediamosa_cql_sub_sortby_add($o_cql_sub_sortby);

        try {
          // Get the next part, if we end here its ok.
          while (1) {
            $str_1 = $this->mediamosa_cql_next($str_cql, $i_pos);

            // Make sure its valid
            $this->mediamosa_cql_must_be_charstring($str_1, $i_pos);

            $o_cql_sub_sortby = new mediamosa_cql_sub_sortby($str_1);

            // do modifiers (if any)
            $this->mediamosa_cql_modifiers_do($str_cql, $i_pos, $o_cql_sub_sortby);

            // Add it to our object
            $this->m_o_cql_sortby->mediamosa_cql_sub_sortby_add($o_cql_sub_sortby);
          }
        }
        catch (mediamosa_cql_exception_eos $e) {
          // this was expected here and ok
          assert($e);
        }
      }
      elseif ($this->mediamosa_cql_context_is_boolean($str_1)) {
        $o_cql_part_boolean = new mediamosa_cql_part_boolean($str_1);

        // do modifiers (if any)
        $this->mediamosa_cql_modifiers_do($str_cql, $i_pos, $o_cql_part_boolean);

        // Set the boolean
        $o_cql_part->mediamosa_cql_part_boolean_set($o_cql_part_boolean);

        // If we end with a boolean, we must 'something' after it.
        $this->mediamosa_cql_has_next($str_cql, $i_pos);
      }
      elseif ($str_1 == ')') {
        // ignore, we handle this in next iteration
      }
      else {
        throw new mediamosa_cql_exception_syntax_error('expecting boolean, operator or sortby', $i_pos_old);
      }
    }
    catch (mediamosa_cql_exception_eos $e) {
      // unexpected!
      assert($e);
      throw new mediamosa_cql_exception_unexpected_eos();
    }

    $o_cql_part_group->mediamosa_cql_part_add($o_cql_part);
  }

  /**
   * Check if next char matches with given char
   *
   * @param string $str_cql
   * @param int $i_pos
   * @return bool
   */
  final private function mediamosa_cql_next_is($str_cql, $i_pos, $char) {

    try {
      $this->mediamosa_cql_skip_ws($str_cql, $i_pos);
      return (mediamosa_unicode::substr($str_cql, $i_pos, 1) == $char ? TRUE : FALSE);
    }
    catch (mediamosa_cql_exception_eos $e) {
      // ignore
      assert($e);
    }

    return FALSE;
  }

  /**
   * Check for modifiers and collect them when present
   *
   * @param string $str_cql
   * @param int $i_pos
   * @param object $cql_modifier
   */
  final private function mediamosa_cql_modifiers_do($str_cql, &$i_pos, $cql_modifier) {

    assert(is_object($cql_modifier));

    try {
      while ($this->mediamosa_cql_next_is($str_cql, $i_pos, '/')) {
        $str_1 = mediamosa_unicode::substr($this->mediamosa_cql_next($str_cql, $i_pos), 1);

        // Check for valid string
        $this->mediamosa_cql_must_be_charstring($str_1, $i_pos);

        // Check if we specified '='.
        $value = '';
        $operator = '';
        if ($this->mediamosa_cql_next_is($str_cql, $i_pos, '=')) {
          $operator = '=';
          // Get the next part.
          $str_3 = $this->mediamosa_cql_next($str_cql, $i_pos);
          if ($str_3 !== '=') {
            throw new mediamosa_cql_exception_expecting_char('=', $i_pos);
          }

          $value = $this->mediamosa_cql_next($str_cql, $i_pos);
        }

        $cql_modifier->mediamosa_cql_modifier_add($str_1, $i_pos, $value, $operator);
      }
    }
    catch (mediamosa_cql_exception_eos $e) {
      assert($e);
    }
  }

  /**
   * Test if its a valid 'string', will throw exception if not
   *
   * @param string $str_1
   */
  final private function mediamosa_cql_must_be_charstring($str_1, $i_pos) {
    if (!$this->mediamosa_cql_is_charstring($str_1)) {
      throw new mediamosa_cql_exception_syntax_error(sprintf('%s is not valid char string', $str_1), $i_pos);
    }
  }

  /**
   * Test if its a valid 'string'
   *
   * @param string $str
   * @return string
   */
  final function mediamosa_cql_is_charstring($str) {
    return ($this->mediamosa_cql_is_charstring_2($str) || $this->mediamosa_cql_is_charstring_1($str));
  }

  final function mediamosa_cql_is_charstring_1($str) {
    if ($str == '') {
      return FALSE;
    }

    $a_invalidchars = array('(', ')', '=', '<', '>', '"', '/', ' ');
    for ($x = 0; $x < count($str); $x++) {
      if (mediamosa_unicode::substr($str, $x, 1) == '\\') {
        $x++;
        continue;
      }

      if (in_array(mediamosa_unicode::substr($str, $x, 1), $a_invalidchars)) {
        return FALSE;
      }
    }

    return TRUE;
  }

  final private function mediamosa_cql_is_charstring_2($str) {
    return ($str != '' && mediamosa_unicode::substr($str, 0, 1) == '"' && mediamosa_unicode::substr($str, -1) == '"' && $str != '""');
  }

  /**
   * Converts a any date into a db mysql date (YYYY-MM-DD HH:MM:SS)
   *
   * Will convert to timezone set in CQL object.
   *
   * @param string $date
   *  The paramter to convert.
   * @param string $timezone
   *  Supply timezone for conversion.
   */
  protected function mediamosa_cql_str_date_2_db_date($date, $timezone = 'UTC', $format = 'Y-m-d H:i:s') {
    // Make the date valid for MySQL;
    $date_default = '0000-01-01T00:00:00';
    $date = $date . mediamosa_unicode::substr($date_default, mediamosa_unicode::strlen($date));

    // Get current timezone.
    $tz = date_default_timezone_get();

    try {
      // Parse the date, date_parse is new in 5.2.
      $date_parsed = date_parse($date);

      // Set to timezone of app.
      assert(date_default_timezone_set($timezone));

      // Convert to integer
      $time = mktime($date_parsed['hour'], $date_parsed['minute'], $date_parsed['second'], $date_parsed['month'], $date_parsed['day'], $date_parsed['year']);

      // Back to GMT date again, make it valid.
      $result = gmdate($format, $time);
    }
    catch (Exception $e) {
      // Make sure we return to old time zone.
      date_default_timezone_set($tz);
      throw $e;
    }

    // Back to original timezone, so we dont screw with other time functions.
    date_default_timezone_set($tz);

    return $result;
  }

  /**
   * Convert the CQL array result to a string.
   *
   * @param array $parse_result
   *   The parse result to convert to string.
   * @param string $outside
   *   The last conjuction type of our parent.
   * @param bool $first
   *   Is first call, or child call.
   *
   * @return string
   */
  protected function mediamosa_cql_parseresult_to_string(array $parse_result, $outside = null, $first = true) {
    $inside = array();
    foreach ($parse_result as &$item) {
      if (!is_string($item)) {
        continue;
      }

      $item = trim($item);

      foreach (array(' AND', ' OR') as $conjunction) {
        if (mediamosa_unicode::strlen($item) > $conjunction && mediamosa_unicode::substr($item, -mediamosa_unicode::strlen($conjunction)) === $conjunction) {
          $inside[] = $conjunction;
        }
      }
    }

    $inside = array_unique($inside);
    $inside = count($inside) > 1 ? null : reset($inside);

    foreach ($parse_result as &$item) {
      if (!is_string($item)) {
        $item = $this->mediamosa_cql_parseresult_to_string($item, $inside, false);
      }
    }

    $str_ql = implode(' ', $parse_result);

    // Fix conjunction bug.
    if ($str_ql === 'OR' || $str_ql === 'AND') {
      $str_ql = '';
    }

    // The end can not end with a conjuction.
    while (true) {
      if (mediamosa_unicode::substr($str_ql, -mediamosa_unicode::strlen(' OR')) === ' OR') {
        $str_ql = mediamosa_unicode::substr($str_ql, 0, -mediamosa_unicode::strlen(' OR'));
      }
      elseif (mediamosa_unicode::substr($str_ql, -mediamosa_unicode::strlen(' AND')) === ' AND') {
        $str_ql = mediamosa_unicode::substr($str_ql, 0, -mediamosa_unicode::strlen(' AND'));
      }
      else {
        break;
      }
    }

    if (!$first && (is_null($outside) || is_null($inside) || $outside !== $inside)) {
      $str_ql = '(' . $str_ql . ')';
    }

    return $str_ql;
  }

  /**
   * Remove the ^ from the string.
   *
   * @param string $string
   */
  protected function mediamosa_cql_trim_anchor($string) {
    if (mediamosa_unicode::substr($string, 0, 1) === '^' && mediamosa_unicode::substr($string, -1) === '^' && mediamosa_unicode::substr(str_replace('\\', '', $string), -2) !== '\^') {
      return mediamosa_unicode::substr($string, 1, -1);
    }
    elseif (mediamosa_unicode::substr($string, 0, 1) === '^') {
      return mediamosa_unicode::substr($string, 1);
    }
    elseif (mediamosa_unicode::substr($string, -1) === '^' && mediamosa_unicode::substr(str_replace('\\', '', $string), -2) !== '\^') {
      return mediamosa_unicode::substr($string, 0, -1);
    }

    return $string;
  }
}

/**
 * Class modifiers is used a extension on cql_parts and on sortby object
 *
 */
abstract class mediamosa_cql_modifiers {

  abstract protected function mediamosa_cql_modifier_allowed_modifier($str);

  protected $m_a_str_modifiers = array();

  /**
   * Add the found modifier and its data.
   *
   * @param string $str_modifier
   *   The name of the modifier in lowercase.
   * @param int $i_pos
   *   The position where the modifier was found in the CQL, used for exception.
   * @param string $value
   *   (optional) The value of the modifier.
   * @param string $operator
   *   (optional) The operator (e.g. '=').
   */
  public function mediamosa_cql_modifier_add($str_modifier, $i_pos, $value, $operator) {
    assert(mediamosa_unicode::substr($str_modifier, 0, 1) != '/');
    if (!$this->mediamosa_cql_modifier_allowed_modifier(mediamosa_unicode::strtolower($str_modifier))) {
      throw new mediamosa_cql_exception_syntax_error(sprintf('%s is not an allowed modifier', $str_modifier), $i_pos - mediamosa_unicode::strlen($str_modifier));
    }

    $str_modifier = mediamosa_unicode::strtolower($str_modifier);
    $this->m_a_str_modifiers[$str_modifier] = array('modifier' => $str_modifier, 'operator' => $operator, 'value' => $value);
  }

  /**
   * Test if there are any modifiers.
   *
   * @return bool
   *   Return TRUE for 1 or more modifiers, FALSE otherwise.
   */
  public function mediamosa_cql_modifier_has() {
    return !empty($this->m_a_str_modifiers);
  }

  /**
   * Retrieve modifier array.
   *
   * @return array
   */
  public function mediamosa_cql_modifier_get_array() {
    return $this->m_a_str_modifiers;
  }
}

/**
 * Class sortby handles the sorting parameters in the cql string.
 */
class mediamosa_cql_sortby {
  private $m_a_sub_sortby = array();

  final public function mediamosa_cql_sub_sortby_add($str) {
    $this->m_a_sub_sortby[] = $str;
  }

  final public function mediamosa_cql_sub_sortby_parse($o_obj, $function_name, array $a_prefix_assignments = array(), array $array_settings = array()) {
    $a_results = array();

    // Walk through my sortby's
    foreach ($this->m_a_sub_sortby as $o_sub_sortby) {
      $str_result = $o_obj->$function_name($o_sub_sortby);

      assert($str_result != "");

      $a_results[] = $str_result;
    }

    return $a_results;
  }
}

/**
 * Sub class for multi sort bys
 *
 */
class mediamosa_cql_sub_sortby extends mediamosa_cql_modifiers {

  private $m_str_sort_by = "";

  final public function __construct($str_sort_by) {
    $this->m_str_sort_by = $str_sort_by;
  }

  /**
   * return the modifier string
   *
   * @return string
   */
  final public function mediamosa_cql_sort_by_get() {
    return $this->m_str_sort_by;
  }

  protected function mediamosa_cql_modifier_allowed_modifier($str) {
    return in_array($str, array('sort.descending', 'sort.ascending', 'descending', 'ascending')) ? TRUE : FALSE;
  }
}
