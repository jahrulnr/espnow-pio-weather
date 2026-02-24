#pragma once
#include "SpiAllocator.h"

class SpiJsonDocument : public ArduinoJson::JsonDocument {
public:
    /**
     * @brief Construct a new SpiJsonDocument
     */
    SpiJsonDocument() : ArduinoJson::JsonDocument(SpiAllocator::instance()) {
        // In ArduinoJson 7, the capacity is managed dynamically
    }

    /**
     * @brief Construct from a JsonVariant
     * @param src The JsonVariant to copy
     */
    SpiJsonDocument(const ArduinoJson::JsonVariant& src) 
        : ArduinoJson::JsonDocument(SpiAllocator::instance()) {
        this->set(src);
    }
    
    /**
     * @brief Get the capacity of the document (for backward compatibility)
     * @return The capacity in bytes
     */
    size_t capacity() const {
        return this->size();
    }
};