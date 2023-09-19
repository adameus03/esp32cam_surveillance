#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"

void listFiles(String dirName, String* output) {
  File root = SD_MMC.open(dirName);
  *output += String("HELLOOO");
  Serial.println("HAYLLOOOOOOO\nOOOOOOOOO\nOOOOOOO\nOOOOOOOO\r\nOOOOOOOOOOOOOOOOOOOOOOOOOOOOoo\r\n");

  Serial.println("if (!root)"); //D
  if (!root) {
    *output += String("Failed to open directory");
    return;
  }

  Serial.println("if (!root.isDirectory())"); //T
  if (!root.isDirectory()) {
    *output += String("Not a directory");
    return;
  }

  Serial.println("File file = root.openNextFile();"); //D
  File file = root.openNextFile();

  Serial.println("while (file)"); //D
  while (file) {
    Serial.println("Inside while(file) loop)"); //D
    *output += String("<p>");
    if (file.isDirectory()) {
      *output += String("DIR: ");
      Serial.print("S-DIR: ");//D
    } else {
      *output += String("FILE: ");
      Serial.print("S-FILE: ");//D
    }
    *output += String(file.name() + String("</p>"));
    Serial.println(file.name()); //D
    file = root.openNextFile();
  }
}