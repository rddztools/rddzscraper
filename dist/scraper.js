var system = require("system");
var page = require("webpage").create();

phantom.clearCookies();

if (system.args.length === 2) {
    console.log("Try to pass some args when invoking this script!");
    phantom.exit(1);
} else {
    var cookieKey = "";
    var cookieValue = "";
    var render = false;
    var cookieFileName = "";
    var referrer = "";
    var clipRectFilnemane = "";
    var userAgent = "";
    var pageTimeout = 0;
    page.onError = function (e, t) {};

 page.onResourceError = function(resourceError) {
  console.log('Unable to load resource (#' + resourceError.id + 'URL:' + resourceError.url + ')');
  console.log('Error code: ' + resourceError.errorCode + '. Description: ' + resourceError.errorString);
};
	    
    page.onResourceTimeout = function(e) {
      console.log("Timeout : " + e.errorCode + " " +e.errorString + " " + e.url);   // it'll probably be 408
    //  console.log(e.errorString); // it'll probably be 'Network timeout on resource'
    //  console.log(e.url);         // the url whose request timed out
      phantom.exit(1);
    };

    
    var url = "" + system.args[1] + "";
    system.args.forEach(function (e, t) {
        if (e === "render=true") {
            render = true;
        }
        if (e.indexOf("cookieFilename=") !== -1) {
            cookieFileNameArg = e.split("=");
            cookieFileName = cookieFileNameArg[1]
        }

        if (e.indexOf("clipRect=") !== -1) {
            clipRectFilnemane = e.substr(e.indexOf("=")+1);
        }
        if (e.indexOf("referrerUrl=") !== -1) {
            referrer = e.substr(e.indexOf("=")+1);
        }
        if(e.indexOf("userAgent=") !== -1){
            userAgent = e.substr(e.indexOf("=")+1);
        }
        if(e.indexOf("pageLoadTimeout=") !== -1){
            pageTimeout = e.substr(e.indexOf("=")+1);
        }

    });

    
   // var urlParser = document.createElement('a');
    //urlParser.href = url;
   // var hostData = urlParser.hostname;
    
   page.settings.userAgent = ""+userAgent+"";
    //~ page.settings.userAgent = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_3) AppleWebKit/534.53.11 (KHTML, like Gecko) Version/5.1.3 Safari/534.53.10";
   //~ page.settings.userAgent = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:40.0) Gecko/20100101 Firefox/40.1";
    //page.settings.userAgent = "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:40.0) Gecko/20100101 Firefox/40.1";
    page.settings.resourceTimeout = pageTimeout;
    
    //~ console.log(userAgent);
    //~ console.log(page.settings.userAgent);
    
//    page.customHeaders = {
//        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
//        "Accept-Language": "fr,fr-fr;q=0.8,en-us;q=0.5,en;q=0.3",
//        "Referer": referrer,
//        "Host": hostData
//    };
    
    //~ var url = 'https://www.google.fr/?gws_rd=ssl#q=pizza';
    
    page.open(url, function(e) {
        if (e === "success") {
               console.log(page.content);
                if(render === true)
                {
                page.clipRect = {
                    top: 132,
                    left: 29,
                    width: 200,
                    height: 70
                };
                page.render(clipRectFilnemane)
                }
		page.close();
                phantom.exit(0);
		//setTimeout(function(){
		//console.log(page.content);
               // page.close();
               // phantom.exit(0);
		//},10000);
        }
        else
        {
		//~ console.log("Unable to open url " + url);
		//~ console.log("HTTP Status  " + e);
		//~ console.log("HTTP Status  " + e.errorCode);
        phantom.exit(1);
        }
    });
}
