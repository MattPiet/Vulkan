#ifndef __ML_H__
#define __ML_H__

#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <iostream>

namespace ML
{
    template <typename DataType>
      class List
    {
    private:
        size_t size;
        size_t capacity;
        DataType* data;

    public:
        
        List(size_t initialCapacity = 256) : size(0), capacity(initialCapacity) 
        {
            if (capacity == 0) capacity = 256;
            data = static_cast<DataType*>(std::malloc(capacity * sizeof(DataType)));
        }
        
        List(const List&) = delete;
        List& operator=(const List&) = delete;

        ~List() 
        {
            std::free(data);
        }

        void reserve(size_t newCapacity) 
        {
            if (newCapacity > capacity) 
            {
                DataType* newBlock = static_cast<DataType*>(std::realloc(data, newCapacity * sizeof(DataType)));
                if (!newBlock) {
                    std::cerr << "Fatal: Failed to reallocate buffer!" << std::endl;
                    return;
                }
                data = newBlock;
                capacity = newCapacity;
            }
        }

        inline void push_back(const DataType& newData)
        {
            if (size == capacity)
            {
                reserve(capacity * 2);
            }
            data[size] = newData;
            size++;
        }
        
        void shrink_to_fit()
        {
            if (capacity == size) return;

            if (size == 0) 
            {
                destroy();
                return;
            }

            DataType* exactBlock = static_cast<DataType*>(std::realloc(data, size * sizeof(DataType)));
            if (exactBlock) 
            {
                data = exactBlock;
                capacity = size;
            }
        }

        inline DataType* getData() const { return data; }
        inline size_t getSize() const { return size; }
        inline size_t getCapacity() const { return capacity; }
    
        inline void clear()
        {
            size = 0;        
        }

        void destroy()
        {
            std::free(data); 
            data = nullptr;  
            size = 0;        
            capacity = 0;    
        }
    };
    
template <typename K, typename V, typename Hash = std::hash<K>>
    class Map
    {
    private:
        struct Entry {
            
            alignas(std::pair<K, V>) char storage[sizeof(std::pair<K, V>)];

            K& key() {
                return reinterpret_cast<std::pair<K, V>*>(storage)->first;
            }
            const K& key() const {
                return reinterpret_cast<const std::pair<K, V>*>(storage)->first;
            }
            V& value() {
                return reinterpret_cast<std::pair<K, V>*>(storage)->second;
            }
            const V& value() const {
                return reinterpret_cast<const std::pair<K, V>*>(storage)->second;
            }
        };
        
        Entry* table;
        uint8_t* occupied;
        size_t capacity;
        size_t mask;
        size_t size;

        void rehash(size_t newCapacity) 
        {
            Entry* oldTable = table;
            uint8_t* oldOccupied = occupied;
            size_t oldCapacity = capacity;

            capacity = newCapacity;
            mask = capacity - 1;
            
            table = static_cast<Entry*>(std::malloc(capacity * sizeof(Entry)));
            occupied = static_cast<uint8_t*>(std::calloc(capacity, sizeof(uint8_t)));
            size_t oldSize = size;
            size = 0; // Will be re-incremented during re-insertion

            Hash hasher;

            for (size_t i = 0; i < oldCapacity; ++i) {
                if (oldOccupied[i]) {
                    // Hash using standard library or custom hasher
                    size_t h = hasher(oldTable[i].key());
                    size_t slot = h & mask;
                    
                    while (occupied[slot]) {
                        slot = (slot + 1) & mask;
                    }
                    
                    occupied[slot] = 1;
                    // Move-construct into new table
                    ::new (static_cast<void*>(&table[slot])) Entry();
                    table[slot].key() = std::move(oldTable[i].key());
                    table[slot].value() = std::move(oldTable[i].value());

                    // Destroy old entry objects
                    oldTable[i].key().~K();
                    oldTable[i].value().~V();
                    
                    size++;
                }
            }

            std::free(oldTable);
            std::free(oldOccupied);
        }

    public:
        Map(size_t initialCapacity = 256) : size(0) 
        {
            capacity = 1;
            while (capacity < initialCapacity) {
                capacity *= 2;
            }
            if (capacity < 16) capacity = 16;
            mask = capacity - 1; 
            
            table = static_cast<Entry*>(std::malloc(capacity * sizeof(Entry)));
            occupied = static_cast<uint8_t*>(std::calloc(capacity, sizeof(uint8_t)));
        }
        
        ~Map() 
        {
            clear();
            std::free(table);
            std::free(occupied);
        }

        Map(const Map&) = delete;
        Map& operator=(const Map&) = delete;

        void clear() {
            for (size_t i = 0; i < capacity; ++i) {
                if (occupied[i]) {
                    table[i].key().~K();
                    table[i].value().~V();
                }
            }
            std::memset(occupied, 0, capacity * sizeof(uint8_t));
            size = 0;
        }

        inline bool findOrInsert(const K& key, const V& newValue, V& outValue) 
        {
            if (size + 1 > (capacity * 3) / 4) {
                rehash(capacity * 2);
            }

            Hash hasher;
            size_t h = hasher(key);
            size_t slot = h & mask; 
            
            while (occupied[slot]) 
            {
                if (table[slot].key() == key) {
                    outValue = table[slot].value();
                    return false; 
                }
                slot = (slot + 1) & mask; 
            }
            
            occupied[slot] = 1;
            // Construct the key and value in-place using placement new
            ::new (static_cast<void*>(&table[slot])) Entry();
            table[slot].key() = key;
            table[slot].value() = newValue;
            outValue = newValue;
            size++;
            return true; 
        }

    inline const V* find(const K& key) const 
        {
            Hash hasher;
            size_t h = hasher(key);
            size_t slot = h & mask; 
            
            while (occupied[slot]) 
            {
                if (table[slot].key() == key) {
                    return &table[slot].value(); 
                }
                slot = (slot + 1) & mask; 
            }
            
            return nullptr; 
        }

        inline size_t getSize() const { return size; }
        inline size_t getCapacity() const { return capacity; }
    };
    template <typename K, typename V>
    class FlatMap
    {

    private:
        struct Entry {
            K key;
            V value;
        };

        Entry* table;
        uint8_t* occupied;
        size_t capacity;
        size_t mask;
        size_t size;

        static inline uint32_t hashBytes(const void* data, size_t length) {
            const uint8_t* bytePtr = static_cast<const uint8_t*>(data);
            uint32_t hash = 2166136261u;
            for (size_t i = 0; i < length; ++i) {
                hash ^= bytePtr[i];
                hash *= 16777619u;
            }
            return hash;
        }

        void rehash(size_t newCapacity) 
        {
            Entry* oldTable = table;
            uint8_t* oldOccupied = occupied;
            size_t oldCapacity = capacity;

            capacity = newCapacity;
            mask = capacity - 1;
            
            table = static_cast<Entry*>(std::malloc(capacity * sizeof(Entry)));
            occupied = static_cast<uint8_t*>(std::calloc(capacity, sizeof(uint8_t)));
            size = 0;

            for (size_t i = 0; i < oldCapacity; ++i) {
                if (oldOccupied[i]) {
                    uint32_t h = hashBytes(&oldTable[i].key, sizeof(K));
                    size_t slot = h & mask;
                    
                    while (occupied[slot]) {
                        slot = (slot + 1) & mask;
                    }
                    
                    occupied[slot] = 1;
                    table[slot] = oldTable[i];
                    size++;
                }
            }

            std::free(oldTable);
            std::free(oldOccupied);
        }

    public:
        FlatMap(size_t initialCapacity = 256) : size(0) 
        {
            capacity = 1;
            while (capacity < initialCapacity) {
                capacity *= 2;
            }
            if (capacity < 16) capacity = 16;
            mask = capacity - 1; 
            
            table = static_cast<Entry*>(std::malloc(capacity * sizeof(Entry)));
            occupied = static_cast<uint8_t*>(std::calloc(capacity, sizeof(uint8_t)));
        }
        
        ~FlatMap() 
        {
            std::free(table);
            std::free(occupied);
        }

        FlatMap(const FlatMap&) = delete;
        FlatMap& operator=(const FlatMap&) = delete;

        void clear() {
            std::memset(occupied, 0, capacity * sizeof(uint8_t));
            size = 0;
        }

        inline bool findOrInsert(const K& key, const V& newValue, V& outValue) 
        {
            // 75% load factor threshold for bulk loading efficiency
            if (size + 1 > (capacity * 3) / 4) {
                rehash(capacity * 2);
            }

            uint32_t h = hashBytes(&key, sizeof(K));
            size_t slot = h & mask; 
            
            while (occupied[slot]) 
            {
                if (std::memcmp(&table[slot].key, &key, sizeof(K)) == 0) {
                    outValue = table[slot].value;
                    return false; 
                }
                slot = (slot + 1) & mask; 
            }
            
            occupied[slot] = 1;
            table[slot].key = key;
            table[slot].value = newValue;
            outValue = newValue;
            size++;
            return true; 
        }

        inline V* find(const K& key) 
        {
            uint32_t h = hashBytes(&key, sizeof(K));
            size_t slot = h & mask; 
            
            while (occupied[slot]) 
            {
                if (std::memcmp(&table[slot].key, &key, sizeof(K)) == 0) {
                    return &table[slot].value; 
                }
                slot = (slot + 1) & mask; 
            }
            
            return nullptr; 
        }

        inline size_t getSize() const { return size; }
        inline size_t getCapacity() const { return capacity; }
    };
}

#endif