#include "ESP8266TrueRandom.h"
#include "ArduinoJson.h"
#include "SD.h"
#include "SPI.h"
#include "db.h"

database::database(int chipSelect){
	if(!SD.begin(chipSelect)) Serial.println("SD card connection failed.");
}

String database::insert(String data){
	String id = uniqueId();
	writeFile(id, data);
	return id;
}

String database::remove(String id){
	if(exists(id)){
		char idc[9];
		id.toCharArray(idc, 8);
		SD.remove(idc);
	    return id;
	}else return "0";
}

String database::update(String id, String data){
	if(exists(id)){
		writeFile(id, data);
	    return id;
	}else return "0";
}

String database::find(String id){
	if(exists(id)){
		String output = "";
		File f;
		f = SD.open(id.c_str());
		while(f.available()){
			char c = f.read();
			output += c;
		}
	    f.close();
	    return output;
	}else return "0";
}

bool database::exists(String path){
	File f;
	f = SD.open(path.c_str());
	bool a = f.available();
	f.close();
	return a;
}

String database::uniqueId(){
	randomSeed(millis()+analogRead(A0));
  	String hexString = "";
  	for (int i = 0; i < 4; i++) {
    	hexString += String(random(256), HEX); //fill string with hex
  	}
  	char idc[9];
	hexString.toCharArray(idc, 8);
	if(exists(hexString)) return uniqueId();
  	else return hexString; //success
}

bool database::writeFile(String path, String data){
	File f;
	f = SD.open(path.c_str(), FILE_WRITE);
	f.seek(0);
	f.print(data);
    f.close();
}