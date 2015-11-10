#include <ESP8266TrueRandom.h>
#include <ArduinoJson.h>
#include <db.h>
#include <SPI.h>
#include <SD.h>

void setup(){
  Serial.begin(9600);
  while (!Serial) {
    ;
  }
  database db = database(2);
  /* proceed with tests */

  /* insert and retrieve */
  Serial.println("\n\n1. insert and retrieve");
  String id = db.insert("{}");
  Serial.println("  got id: "+id);
  Serial.println("  retrieveing:");
  String data = db.find(id);
  Serial.println("  got data: "+data);
  if(data == "{}") Serial.println("success");
  else Serial.println("fail");

   /* update and retrieve */
  Serial.println("\n2. update and retrieve");
  String updated = db.update(id, "{\"new\":\"data\"}");
  if(id == "0") Serial.println("update failed");
  else{
    Serial.println("  updated id: "+id);  
    String data = db.find(id);
    Serial.println("  got new data: "+data);
    if(data == "{\"new\":\"data\"}") Serial.println("success");
    else Serial.println("fail");
  }

   /* exists */
  Serial.println("\n3. exists after insert");
  bool exists = db.exists(id);
  Serial.println("  checking: "+id);
  if(!exists) Serial.println("fail");
  else Serial.println("success");

   /* no exist */
  Serial.println("\n4. unknown does not exist");
  bool noexist = db.exists("00000000");
  Serial.println("  checking: 00000000");
  if(noexist) Serial.println("fail");
  else Serial.println("success");

  /* removal */
  Serial.println("\n5. removal");
  Serial.println("  removing: "+id);
  String removed = db.remove(id);
  Serial.println("  removed: "+removed);
  if(removed == id) Serial.println("success");
  else Serial.println("fail");

  /* no exist after removal */
  Serial.println("\n6. removed does not exist");
  noexist = db.exists(id);
  Serial.println("  checking removed: "+id);
  if(noexist) Serial.println("fail");
  else Serial.println("success");

  /* remove twice */
  Serial.println("\n7. remove already removed");
  Serial.println("  removing: "+id);
  removed = db.remove(id);
  Serial.println("  removed: "+removed);
  if(removed != id) Serial.println("success");
  else Serial.println("fail");

  /* find non existing */
  Serial.println("\n8. find absent");
  Serial.println("  finding id: 000000");  
  data = db.find("000000");
  Serial.println("  got: "+data);  
  if(data == "0") Serial.println("success");
  else Serial.println("fail");

  /* update non existing */
  Serial.println("\n8. update absent");
  Serial.println("  updating id: 000000");  
  updated = db.update("000000", "{}");
  Serial.println("  updated: "+updated);  
  if(updated == "0") Serial.println("success");
  else Serial.println("fail");
}

void loop()
{
  // nothing happens after setup
}

