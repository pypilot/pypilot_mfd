/*  Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  
*/

document.getElementById("otafile").focus();  // so simply hitting enter opens it

function startUpload() {
    var otafile = document.getElementById("otafile").files;

    if (otafile.length == 0) {
	alert("No file selected!");
    } else {
	document.getElementById("otafile").disabled = true;
        
	var file = otafile[0];
	var xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function() {
	    if (xhr.readyState == 4) {
		if (xhr.status == 200) {
		    document.open();
		    document.write(xhr.responseText);
		    document.close();
		} else if (xhr.status == 0) {
		    alert("Server closed the connection abruptly!");
		    location.reload()
		} else {
		    alert(xhr.status + " Error!\n" + xhr.responseText);
		    location.reload()
		}
	    }
	};
        
	xhr.upload.onprogress = function (e) {
            var percent = (e.loaded / e.total * 100).toFixed(0) + "%";
	    var progresstext = document.getElementById("progresstext");
	    progresstext.textContent = percent;
	    var fileprogress = document.getElementById("fileprogress");
            fileprogress.style.width = percent;
	};
	xhr.open("POST", "/update", true);
        xhr.setRequestHeader("Content-Type", "application/octet-stream");
	xhr.send(file);
    }
}
