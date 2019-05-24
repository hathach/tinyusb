var HID = require('node-hid');
var devices = HID.devices();
var deviceInfo = devices.find( function(d) {
    var isNRF = d.vendorId===0Xcafe && d.productId===0X4004;
    return isNRF;
});
if( deviceInfo ) {
	console.log(deviceInfo)
	var device = new HID.HID( deviceInfo.path );
	device.on("data", function(data) {console.log(data)});

	device.on("error", function(err) {console.log(err)});

	setInterval(function () {
		device.write([0x00, 0x01, 0x01, 0x05, 0xff, 0xff]);
	},500)
}
