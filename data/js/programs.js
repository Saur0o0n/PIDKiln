// Some JS for PID kilin


function ValidateSize(file) {
        var FileSize = file.files[0].size / 1024; // in KiB
        if (FileSize > 10) {
		alert("Program file is too big - limit is 10 KiB !");
		file.value = "";
        }
}

