var uri = ""
var hostname = window.location.hostname;
if(hostname==""){
    hostname="192.168.12.175";
    uri = "http://"+hostname;
}

//on ready
$(function(){
    // ---------------- sockets -----------------
    var socket = new WebSocket('ws://'+hostname+'/ws');

    socket.onmessage = function(evt){
        var obj = JSON.parse(evt.data);
        parseStatus(obj);
        // console.log("I got data: ", evt.data);
    }
    // ------------ UI --------------------
    $(".garage-status").click(function(){
        $(this).addClass("updating");
        if($(this).hasClass("garage-closed")){
            $.get(uri+"/door?action=open");
        }else{
            $.get(uri+"/door?action=open");
        }
    });
    $(".nav-settings, #cancel").click(function(){
        $(".container").slideToggle();
    });
    $("#restart").click(function(){
        $.get(uri+"/restart");
        // window.location.reload();

    });
    $(".nav-settings").click(function(){
        readSettings();
    });
    $("#save").click(function(){
        saveSettings();
    });
});

function parseStatus(obj){
    $(".garage-status").removeClass("updating");
    $(".garage-status, .car").hide();
    $(".garage-status").removeClass("motion");
    if(obj.motion){
        $(".garage-status").addClass("motion");
    }
    if(obj.open){
        $(".garage-open").show();
    } else {
        $(".garage-closed").show();
        if(obj.car){
            $(".car-present").show();
        } else {
            $(".car-notpresent").show();
        }
    }
    //set placeholder
    $("#doorDistanceOpen, #doorDistanceClosed").prop("placeholder", obj.distance);
}

function readSettings(){
    $.ajax({
        dataType: "json",
        url: uri+"/settings",
        success: function(data){
            // console.log(data);
            Object.keys(data).forEach(function(key) {
                if($('#'+key).prop("type")=="checkbox"){
                    $('#'+key).prop("checked", data[key]);
                } else {
                    $('#'+key).val(data[key]);
                }

            });
        }
      });
}

function saveSettings(){
    jdata = {};
    $(".setting input").each(function(){
        if(!this.reportValidity()) return;
        var val = $(this).val();
        if($(this).prop("type")=="number"){
            val = parseInt(val);
        }
        if($(this).prop("type")=="checkbox"){
            val = $(this).prop("checked");
        }
        jdata[$(this).prop("id")]=val;
    });
    $.ajax({
        url: uri+"/settings",
        method: "post",
        // data: {settings: JSON.stringify(jdata)},
        data: {body: JSON.stringify(jdata)},
        // dataType: 'json',
        // contentType: 'application/json',
        success: function(data){
            window.location.reload();
        }
      });
}