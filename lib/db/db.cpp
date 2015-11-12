#include "ArduinoJson.h"
#include "SD.h"
#include "SPI.h"
#include "db.h"

database::database(int chipSelect){
	// File myFile;
	// Serial.println("db init");
	// 
 //  	myFile = SD.open("lol.txt", FILE_WRITE);
 //  	myFile.println("lololllo");
 //  	myFile.close();
 //  	Serial.println("jj");
	
	if(!SD.begin(chipSelect)) Serial.println("SD card connection failed.");
	 Serial.println(find("TEST.JSO"));
}

String database::insert(String data){

}

String database::remove(String id){
	
}

String database::update(String id, String data){
	
}

String database::find(String id){
	Serial.println("finding: "+id);
	String output = "";
	File f;
	f = SD.open(id);
	while(f.available()){
		char c = f.read();
		output += c;
	}
    f.close();
    return output;
}

