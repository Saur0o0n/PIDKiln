// Some JS for PID kilin


function ValidateSize(file) {
        var FileSize = file.files[0].size / 1024; // in KiB
	var Name = file.files[0].name;
	if (Name.length > 20){
		alert("Program name cannot be longer then 20 characters. Sorry - limitation of SPIFFS");
		file.value = "";
	}
        if (FileSize > 10) {
		alert("Program file is too big - limit is 10 KiB !");
		file.value = "";
        }
}

