
let stm = require("stm/client.js");

App({
  onLaunch: function() {
      this.client = new stm.Client();
    this.client.setConnectArgs("ws://www.dddstd.com:10001", function(){
        console.log("connect success");
      }
      , function(str){
        console.error(str);
      });
    this.client.onPush = function(data) {
      console.log(new stm.Client.StringView(data).toString());
    }
  },

  /**
   * @type stm.Client
   */
  client: null
  
})