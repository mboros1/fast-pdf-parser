#pragma once

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <string>
#include <memory>

namespace fast_pdf_parser {

// Type aliases for easier use
using JsonDocument = rapidjson::Document;
using JsonValue = rapidjson::Value;
using JsonAllocator = rapidjson::MemoryPoolAllocator<>;

// Helper class for building JSON documents
class JsonBuilder {
public:
    JsonBuilder() : doc_(std::make_unique<JsonDocument>()) {
        doc_->SetObject();
    }
    
    JsonDocument* document() { return doc_.get(); }
    JsonAllocator& allocator() { return doc_->GetAllocator(); }
    
    // Fast string serialization
    std::string serialize(bool pretty = false) const {
        rapidjson::StringBuffer buffer;
        buffer.Clear();
        
        if (pretty) {
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            doc_->Accept(writer);
        } else {
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc_->Accept(writer);
        }
        
        return std::string(buffer.GetString(), buffer.GetSize());
    }
    
    std::unique_ptr<JsonDocument> release() {
        return std::move(doc_);
    }
    
private:
    std::unique_ptr<JsonDocument> doc_;
};

} // namespace fast_pdf_parser