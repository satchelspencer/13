#include "ESP8266TrueRandom.h"
#include "ArduinoJson.h"
#include "SD.h"
#include "SPI.h"
#include "db.h"

database::database(int chipSelect){
	if(!SD.begin(chipSelect)) Serial.println("SD card connection failed.");
	 Serial.println(insert("{\"you\":78882}"));
}

String database::insert(String data){
	String id = uniqueId();
	writeFile(id, data);
	return id;
}

String database::remove(String id){
	if(SD.exists(id.c_str())){
		SD.remove(id.c_str());
	    return id;
	}else return "0";
}

String database::update(String id, String data){
	Serial.println("updating: "+id);
	if(SD.exists(id.c_str())){
		SD.remove(id.c_str()); //overwrite
		writeFile(id, data);
	    return id;
	}else return "0";
}

String database::find(String id){
	Serial.println("finding: "+id);
	if(SD.exists(id.c_str())){
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

String database::uniqueId(){
	randomSeed(millis()+analogRead(A0));
  	String hexString = "";
  	for (int i = 0; i < 4; i++) {
    	hexString += String(random(256), HEX); //fill string with hex
  	}
  	return hexString; //success
}

bool database::writeFile(String path, String data){
	File f;
	f = SD.open(path.c_str(), FILE_WRITE);
	f.print(data);
    f.close();
}