
function switch_nav() {
  var x = document.getElementById("myTopnav");
  if (x.className === "topnav") {
    x.className += " responsive";
  } else {
    x.className = "topnav";
  }
}


function toggle_password(input) {
  var el = document.getElementById("pass_toggler");

  el.classList.toggle("fa-eye-slash");
  el.classList.toggle("fa-eye");

  if (input.type == "password") {
    input.type = "text";
  } else {
    input.type = "password";
  }
}

