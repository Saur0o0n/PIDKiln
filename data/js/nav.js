
function switch_nav() {
  var x = document.getElementById("myTopnav");
  if (x.className === "topnav") {
    x.className += " responsive";
  } else {
    x.className = "topnav";
  }
}


function toggle_password(input) {
  var name=input.name+"_toggler";
  var el = document.getElementById(name);

  if (input.type == "password") {
    input.type = "text";
    el.src="icons/eyeo.png";
  } else {
    input.type = "password";
    el.src="icons/eyec.png";
  }
}

