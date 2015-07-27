var turl = localStorage.getItem('turl') ? localStorage.getItem('turl') : ''; 
var tcode = localStorage.getItem('tcode') ? localStorage.getItem('tcode') : 'NSW_TP007';
var tlevels = localStorage.getItem('tlevels') ? localStorage.getItem('tlevels') : '0';
// console.log('tlevel from storage: ' + tlevels);
var tdiff;
var tpc;
var highlow;
var lastLevel;
var nextLevel;
var followingLevel;
var followingi;

var turl1 = "http://www.bom.gov.au/australia/tides/print.php?aac=";
var turl2 = "&type=tide&tz=Australia/";
var turl3 = "&days=2&date=";

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function sendResponse() {
  console.log('sendreposne');
      var dictionary = {        
        "KEY_NEXT_TIDE" : tdiff,
        "KEY_HIGHLOW" : highlow,
        "KEY_PC" : tpc,
        "KEY_LAST_LEVEL" : lastLevel,
        "KEY_NEXT_LEVEL" : nextLevel,
        "KEY_FLW_LEVEL" : followingLevel,
        "KEY_CONFIG_LEVELS" : parseInt(tlevels,10)
      };
      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
          console.log("Tide info sent to Pebble successfully!");
        },
        function(e) {
          console.log("Error sending Tide info to Pebble!");
        }
      );
}

var nt = ""; //Next Tide
var t1 = ""; //Tide info
var range;

function getTide() {
  console.log("Get Tide");
  var d = new Date();
//  var url2 = url + d.getDate() + "-" + (d.getMonth()+1) + "-" + d.getFullYear(); // + url1;
  var state = tcode.substring(0,3);
  var city;
    if (state=='NSW')
      city = 'Sydney';
    else if (state=='VIC')
      city = 'Melbourne';
    else if (state=='QLD')
      city = 'Brisbane';
    else if (state=='NT_')
      city = 'Darwin';
    else if (state=='TAS')
      city = 'Hobart';
    else if (state=='WA_')
      city = 'Perth';
    else if (state=='SA_')
      city = 'Adelaide';
  
  var url2 = turl1 + tcode + turl2 + city + turl3 + d.getDate() + "-" + (d.getMonth()+1) + "-" + d.getFullYear();
  console.log(url2);
  var stored_url2 = localStorage.getItem('url2') ? localStorage.getItem('url2') : ''; 
  var stored_responseText = localStorage.getItem('responseText') ? localStorage.getItem('responseText') : ''; 
  if (url2 == stored_url2)  {
    console.log("Cache hit!");
    HTMLTable(stored_responseText);
    sendResponse();
    } else {
      xhrRequest(url2, 'GET', 
      function(responseText) {
        // Assemble dictionary using our keys
        // var response = responseText;
      HTMLTable(responseText);
      sendResponse();
      localStorage.setItem('url2', url2);
      localStorage.setItem('responseText', responseText);
      }      
    );
    }
  
  /* DEBUG - to avoid needlessly calling bom URL
  tdiff = 300;
  highlow = "High";
  tpc = 50;
  lastLevel = "1.0m";
  nextLevel = "2.0m";
  followingLevel = "3.0m";
  sendResponse();
  */
  
}
var r = [];
var s = [];
var t = [];

function HTMLTable(text) {
  var i;
  var d = new Date();
  var addDay = 0;
  var ampm, hrs, mins, diff;
  
  nt = "";
  range = "";
  t1 = "";
  
  // Need to look over 7 iterations for all today's tides (up to 5) and the first 2 tomorrow
  for (i = 0; i < 7; i++) {
    text = text.substring(text.search(/<th rowspan=[^=]*..instance[^>]*tide.>/));
    r[i] = text.substring(text.search(">")+1,text.search(/<\/th>/));
    text = text.substring(text.search(/<td[^=]*..localtime[^>]*>/));
    s[i] = text.substring(text.search(">")+1,text.search(/<\/td>/)).replace(" ","");
    text = text.substring(text.search(/<td[^=]*..height[^>]*>/));
    t[i] = text.substring(text.search(">")+1,text.search(/<\/td>/));
//    console.log(i +"->"+ "r[i]="+r[i]+ " s[i]="+s[i]+ " t[i]="+t[i]);
    
    hrs = Number(s[i].substring(0,s[i].search(":")));
    mins = Number(s[i].substring(s[i].search(":")+1,s[i].search(/[ap]m/)));

    var pmFlag = s[i].search("am") < 0; //True if PM
//    console.log(i + "-> hrs: " + hrs + " mins: " + mins + " pmFlag: " + pmFlag);
    if (!pmFlag && ampm == 12)  //We were previously correcting for PM but now back to AM - we've skipped to the next day
      addDay = 24 * 3600 * 1000; // One day in msec
    else addDay = 0;
//    console.log(i + "-> addDay " + addDay);
    
    if (pmFlag && hrs < 12)  // Must be PM (but only need to adjust when hours go past 12)
      ampm = 12;
    else
      ampm = 0;
    
    if (!pmFlag && hrs == 12) 
      hrs = 0;
//    console.log(i + "-> hrs (after midday correction): " + hrs + " ampm: " + ampm);
//    console.log('Year: '+d.getFullYear()+' Month:'+d.getMonth()+' Date: '+d.getDate()+' hrs: '+(hrs+ampm)+' mins: '+mins);
    var d2 = new Date(d.getFullYear(), d.getMonth(), d.getDate(),  hrs + ampm, mins, 0, 0); // Time of tide
//    console.log('d: '+d+' d2: '+d2);
    diff = addDay + d2.getTime() - d.getTime();
//    console.log ("addDay: " + addDay + " Diff: " + diff);
   if (diff > 0) {
  // Later than now
  
    var hdiff = (diff / 3600000) | 0;
    var mdiff = (((diff/3600000) - hdiff) * 60 ) | 0;
//     console.log(i +"->"+ " Hdiff=" + hdiff + " hdiff /3600000=" +diff/3600000+ " Mdiff=" + mdiff);
    if (nt === "") // We have not yet found the next tide
      {
        tpc = 0;
        if (i > 0) // This is not the first tide of the day so we can work out the range
          {
            var lvl1, lvl2, pc;
            lvl1 = Number(t[i-1].replace("m",""));  // Get rid of the metres symbole
            lvl2 = Number(t[i].replace("m","")); // Get rid of the netres symbol
            pc = Math.abs(lvl1-lvl2) / 1.89 * 100; // Adjust for max possible tide
            range = "\n" + (pc | 0) + "% max range";
 //           console.log("Range: " + range);
            tpc = pc | 0;
            lastLevel = t[i-1].replace(" ","");
          }
        else {
          lastLevel = "n/a";
          range = "";
        }
        nextLevel = t[i].replace(" ","");
        followingi = i+1;
     if (hdiff === 0) // Less than an hour to go
       nt = r[i] + " in " + mdiff + "m";
     else // More than an hour to go
       nt = r[i] + " in " + hdiff + "h " + mdiff + "m";
     nt = nt + range;
        
        highlow = r[i];
        tdiff = hdiff*60 + mdiff; // In Minutes
      }
//    console.log(i +"->"+" Next " + r[i] + " tide in " + hdiff + " hours" + mdiff + " mins");
//    console.log(i + "->" + d.getFullYear() + ":" + d.getMonth()+":"+d.getDate()+":"+d.getHours()+":"+d.getMinutes()+":"+d.getSeconds()+":::" + d.getTime());
//    console.log(i + "->" + d2.getFullYear() + ":" + d2.getMonth()+":"+d2.getDate()+":"+d2.getHours()+":"+d2.getMinutes()+":"+d2.getSeconds()+":::" + d2.getTime());
   }

    t1 = t1 + s[i] + " " + t[i].replace(" ","") + "\n"; // Get rid of needless spaces
  }
//  console.log("followingi: " + followingi);
  followingLevel = t[followingi].replace(" ","");
  if (tcode != "NSW_TP007") // Do the range % calculation for Fort Dennison
    tpc = 0;
}

Pebble.addEventListener('showConfiguration', function(e) {
//  console.log('showConfiguration: tcode:' + tcode);
  var murl = 'http://johnpebble.ucoz.com/TideWatch.html?turl=' + encodeURIComponent(tcode) + '&tcode=' + encodeURIComponent(tcode) +
      '&tlevels=' + encodeURIComponent(tlevels==1?'true':'false');
//  console.log("Settings Url: " + murl);
  Pebble.openURL(murl);
});


// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");
    getTide();  // Delivers the tide reading to the waiting pebble!
  }
);

Pebble.addEventListener('webviewclosed',
  function(e) {
//    console.log('Configuration window returned: ' + e.response);
    var options = JSON.parse(decodeURIComponent(e.response));
    var tmp = decodeURIComponent(options.turl);
    if (tmp != 'undefined')
      turl = tmp;
//    console.log("Settings returned turl: " + tmp);
    localStorage.setItem('turl', turl);    
    tmp = decodeURIComponent(options.tcode);
    if (tmp != 'undefined') 
      tcode = tmp;
//    console.log("Settings returned tcode: " + tmp);
    localStorage.setItem('tcode', tcode);  
    
        tmp = decodeURIComponent(options.tlevels);
    if (tmp != 'undefined') 
      tlevels = tmp=='true'? 1:0;
//    console.log("Settings returned tlevels: " + tmp);
    localStorage.setItem('tlevels', tlevels);  
    
    getTide();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
//    console.log("[JAVASCRIPT] Received message from PEBBLE: " +
//                JSON.stringify(e.payload));
    
    if (typeof e.payload["KEY_GETTIDE"] != "undefined")
      {
        getTide();
      }
  }                     
);

