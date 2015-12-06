#include "ESP8266TrueRandom.h"
#include "ArduinoJson.h"
#include "SD.h"
#include "SPI.h"
#include "db.h"

/**
 * constructor to initialize SD connection
 * @param chipSelect an integer argument sprcifying the chipselect pin to be used for accessing the sd card.
 */
database::database(int chipSelect){
	if(!SD.begin(chipSelect)) Serial.println("SD card connection failed.");
}

/**
 * insert a new record on the SD card, a random id is generated as a save location
 * @param data string to be inserted
 * @return the id of the new document
 */
String database::insert(String data){
	String id = uniqueId();
	writeFile(id, data);
	return id;
}

/**
 * remove a record from the card given the id of a record
 * @param id string id of the record to be removed
 * @return the id of the removed document or "0" of the document does not exist
 */
String database::remove(String id){
	if(exists(id)){
		char idc[9];
		id.toCharArray(idc, 8);
		SD.remove(idc);
	    return id;
	}else return "0";
}

/**
 * update the value given the id of a record
 * @param id string id of the record to be changed
 * @param data the new data to insert
 * @return the id of the removed document or "0" of the document does not exist
 */
String database::update(String id, String data){
	if(exists(id)){
		writeFile(id, data);
	    return id;
	}else return "0";
}

/**
 * get the value of a record given the id
 * @param id string id of the record to be fetched
 * @return the data of the document or "0" of the document does not exist
 */
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

/**
 * check the existence of a document on the SD card
 * @param path on sd card to be checked
 * @return true or false if document exists 
 */
bool database::exists(String path){
	File f;
	f = SD.open(path.c_str());
	bool a = f.available();
	f.close();
	return a;
}

/**
 * generate a unique id that does not exist already on the card. generates a random hex string of length 8 untill it is not on the sd card.
 * @return unique id
 */
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

/**
 * write string data to a path on the card
 * @param path on sd card to be written to / created
 */
bool database::writeFile(String path, String data){
	File f;
	f = SD.open(path.c_str(), FILE_WRITE);
	f.seek(0);
	f.print(data);
    f.close();
}