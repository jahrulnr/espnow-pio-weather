#pragma once
#include <FS.h>
#include <LittleFS.h>
#include <WString.h>
#include <SpiJsonDocument.h>

class DataStore {
public:
  DataStore(){};
  ~DataStore(){};
  
  inline void load(const String& filename = "") {
    if (!LittleFS.exists("/data/")) {
      LittleFS.mkdir("/data");
    }

    if (filename.isEmpty()) {
      _file = LittleFS.open("/data/_default.json", "r+");
    }
    else {
      _file = LittleFS.open("/data/" + filename + ".json", "r+");
    }

    if (!_file) {
      _data = SpiJsonDocument();
      _file.close();
      return;
    }

    auto buffer = (uint8_t*) heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
    String data = "";
    while (_file.available()){
      memset(buffer, 0, 1024);
      size_t readBytes = _file.read(buffer, 1024);
      data += String((const char*) buffer, readBytes);
    }
    delete[] buffer;

    _file.close();
    deserializeJson(_data, data);
  }

  inline SpiJsonDocument data() const { return _data; }

  inline bool save(SpiJsonDocument data) {
    if (LittleFS.exists(_file.name())) {
      LittleFS.remove(_file.name());
    }

    data.shrinkToFit();
    String jsonRaw = data.as<String>();
    _file.write((uint8_t*)jsonRaw.c_str(), jsonRaw.length());

    return _data.set(data);
  }

private:
  File _file;
  SpiJsonDocument _data;

protected:
};