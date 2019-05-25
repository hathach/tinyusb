var HID = require('node-hid');
var devices = HID.devices();
var deviceInfo = devices.find( function(d) {
    var isNRF = d.vendorId===0Xcafe && d.productId===0X4004;
    return isNRF;
});
var reportLen = 64;
if( deviceInfo ) {
	console.log(deviceInfo)
	var device = new HID.HID( deviceInfo.path );
	device.on("data", function(data) { console.log(data.toString('hex')); });

	device.on("error", function(err) {console.log(err)});

	setInterval(function () {
		var buf = Array(reportLen);
		for( var i=0; i<buf.length; i++) {
			buf[i] = 0x30 + i; // 0x30 = '0'
		}
		device.write(buf);
	},500)
}
