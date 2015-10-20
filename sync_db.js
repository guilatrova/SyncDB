(function($) {

  // Re-enable form elements that are disabled for non-ajax situations.
  Drupal.behaviors.enableFormItemsForAjaxForms = {
    attach: function() {
        $('.no-ajax').hide();
        
        $('#sync-db-verify').change(function() {
           if (!$(this).is(":checked")) {
               $('#sync-db-auto').attr("checked", false);               
           }
        });
        
        $('#sync-db-auto').change(function() {
           if ($(this).is(":checked")) {
               $('#sync-db-verify').attr("checked", true);
           }
        });
    }
  };

})(jQuery);
