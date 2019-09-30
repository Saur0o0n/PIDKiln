const valuesUrl = "/PIDKiln_vars.json";

var program_status=0;	// status of the pidkiln program 0-none,1-ready... etc = check pidkiln.h

function dis_bttn(bid){
  $(bid).attr("disabled", true);
}
function ena_bttn(bid){
  $(bid).attr("disabled", false);
}

function dis_all_bttn(){
	dis_bttn("#start_bttn");
	dis_bttn("#end_bttn");
	dis_bttn("#pause_bttn");
	dis_bttn("#abort_bttn");
}

function change_program_status(ns){
  console.log("Program status changed on "+ns);
  if(ns==0){	// program not loaded - disable all buttons
	dis_all_bttn();
  }else if(ns==1){	// program ready - enable run button
	dis_all_bttn();
	ena_bttn("#start_bttn");
  }else if(ns==2){	// program running - enable pause, abort, stop
	dis_all_bttn();
	ena_bttn("#pause_bttn");
	ena_bttn("#end_bttn");
	ena_bttn("#abort_bttn");
	if(!chart_update_id) chart_update_id=setTimeout(chart_update, 30000);
  }else if(ns==3){	// program paused - enable start, abort, stop
	dis_all_bttn();
	ena_bttn("#start_bttn");
	ena_bttn("#end_bttn");
	ena_bttn("#abort_bttn");
	if(!chart_update_id) chart_update_id=setTimeout(chart_update, 30000);
  }else{		// program stopped, aborted, failed - but loaded, enable start
	dis_all_bttn();
	ena_bttn("#start_bttn");
	clearTimeout(chart_update_id);
  }
  program_status=ns;
}

function executeQuery() {
 $.ajax({
   url : valuesUrl,
   dataType : 'json'
 })
 .done((res) => {
   if(res.program_status!=program_status) change_program_status(res.program_status);

   res.pidkiln.forEach(el => { $(el.html_id).val(el.value);  })

 })

 setTimeout(executeQuery, 5000);
}


$(document).ready(function() {
  // run the first time; all subsequent calls will take care of themselves
  executeQuery();
  setTimeout(executeQuery, 5000);
});

change_program_status(1);   // assume program is ready on start
