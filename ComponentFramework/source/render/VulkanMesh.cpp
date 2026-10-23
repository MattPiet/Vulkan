#include "render/VulkanRenderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "core/tiny_obj_loader.h"
#include <core/Profiling.h>
#include <filesystem>
#include <cstring>

IndexedVertexBuffer VulkanRenderer::LoadModelIndexed(const char* filename) {
    std::string name = "IndexedVertexBuffer, " + static_cast<std::string>(filename);
    MonitorTimingStart(name.c_str());
    IndexedVertexBuffer indexedVertexBuffer{};
    ML::List<Vertex> vertices;
    ML::List<uint32_t> indices;
    //std::vector<Vertex> vertices;
    //std::vector<uint32_t> indices;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    
    
std::string Filename = filename;
std::string binFilename = Filename + ".bin"; 
bool loaded = false;

if (std::filesystem::exists(binFilename) && std::filesystem::file_size(binFilename) > 0) 
{
    // Load the entire binary file into memory at once
    std::vector<char> buffer = readFile(binFilename);
    size_t offset = 0;

    // Quick helper lambda to copy bytes from the buffer and advance the offset
    auto readBytes = [&](void* dest, size_t size) {
        std::memcpy(dest, buffer.data() + offset, size);
        offset += size;
    };

    size_t verticesSize, normalsSize, uvSize, numShapes;

    readBytes(&verticesSize, sizeof(size_t));
    attrib.vertices.resize(verticesSize);
    readBytes(attrib.vertices.data(), verticesSize * sizeof(float));

    readBytes(&normalsSize, sizeof(size_t));
    attrib.normals.resize(normalsSize);
    readBytes(attrib.normals.data(), normalsSize * sizeof(float));

    readBytes(&uvSize, sizeof(size_t));
    attrib.texcoords.resize(uvSize);
    readBytes(attrib.texcoords.data(), uvSize * sizeof(float));

    // Read Shapes (Indices)
    readBytes(&numShapes, sizeof(size_t));
    shapes.resize(numShapes);
    for (auto& shape : shapes) {
        size_t iSize;
        readBytes(&iSize, sizeof(size_t));
        shape.mesh.indices.resize(iSize);
        readBytes(shape.mesh.indices.data(), iSize * sizeof(tinyobj::index_t));
    }

    loaded = true;
}

// If the .bin didn't exist, parse the text file and create the .bin
if (!loaded) 
{
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, Filename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // Still use ofstream for writing, it's the cleanest way to dump memory to disk
    std::ofstream out(binFilename, std::ios::binary);

    size_t verticesSize = attrib.vertices.size();
    out.write(reinterpret_cast<const char*>(&verticesSize), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(attrib.vertices.data()), verticesSize * sizeof(float));

    size_t normalsSize = attrib.normals.size();
    out.write(reinterpret_cast<const char*>(&normalsSize), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(attrib.normals.data()), normalsSize * sizeof(float));

    size_t uvSize = attrib.texcoords.size();
    out.write(reinterpret_cast<const char*>(&uvSize), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(attrib.texcoords.data()), uvSize * sizeof(float));

    // Write Shapes (Indices)
    size_t numShapes = shapes.size();
    out.write(reinterpret_cast<const char*>(&numShapes), sizeof(size_t));
    for (const auto& shape : shapes) {
        size_t indicesSize = shape.mesh.indices.size();
        out.write(reinterpret_cast<const char*>(&indicesSize), sizeof(size_t));
        out.write(reinterpret_cast<const char*>(shape.mesh.indices.data()), indicesSize * sizeof(tinyobj::index_t));
    }

    out.close();
    loaded = true;
}
    /*if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename)) {
        throw std::runtime_error(warn + err);
    }*/
    if (!loaded)
    {
        throw std::runtime_error("Could not load Mesh File: " + static_cast<std::string>(filename) + ".");
    }
    size_t maxVertices = attrib.vertices.size() / 3;
    vertices.reserve(maxVertices);
    ML::FlatMap<Vertex, uint32_t> uniqueVertices(maxVertices);
    
    size_t totalIndices = 0;
    for (const auto& shape : shapes) {
        totalIndices += shape.mesh.indices.size();
    }
    indices.reserve(totalIndices);
   
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
               attrib.normals[3 * index.normal_index + 0],
               attrib.normals[3 * index.normal_index + 1],
               attrib.normals[3 * index.normal_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            
            uint32_t targetIndex;
            if (uniqueVertices.findOrInsert(vertex, static_cast<uint32_t>(vertices.getSize()), targetIndex)) {
                vertices.push_back(vertex);
            }
            indices.push_back(targetIndex);
        }
    }
    CreateVertexBuffer(indexedVertexBuffer, vertices);
    CreateIndexBuffer(indexedVertexBuffer, indices);
    
    MonitorTimingEnd(name.c_str());
    return indexedVertexBuffer;
}


void VulkanRenderer::CreateVertexBuffer(IndexedVertexBuffer& indexedBufferMemory, const std::vector<Vertex>& vertices) {
    indexedBufferMemory.vertBufferLength = vertices.size();
    VkDeviceSize bufferSize = indexedBufferMemory.vertBufferLength * sizeof(Vertex);

    BufferMemory stagingBuffer;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexedBufferMemory.vertBufferID, indexedBufferMemory.vertBufferMemoryID);

    CopyBuffer(stagingBuffer.bufferID, indexedBufferMemory.vertBufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::CreateVertexBuffer(IndexedVertexBuffer& indexedBufferMemory, const ML::List<Vertex>& vertices) {
    indexedBufferMemory.vertBufferLength = vertices.getSize() ;
    VkDeviceSize bufferSize = indexedBufferMemory.vertBufferLength * sizeof(Vertex);

    BufferMemory stagingBuffer;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);
    memcpy(data, vertices.getData(), bufferSize);
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexedBufferMemory.vertBufferID, indexedBufferMemory.vertBufferMemoryID);

    CopyBuffer(stagingBuffer.bufferID, indexedBufferMemory.vertBufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::CreateIndexBuffer(IndexedVertexBuffer& indexedBufferMemory, const std::vector<uint32_t>& indices) {
    indexedBufferMemory.indexBufferLength = indices.size();
    VkDeviceSize bufferSize = indexedBufferMemory.indexBufferLength * sizeof(uint32_t);
    BufferMemory stagingBuffer{};

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexedBufferMemory.indexBufferID, indexedBufferMemory.indexBufferMemoryID);

    CopyBuffer(stagingBuffer.bufferID, indexedBufferMemory.indexBufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::CreateIndexBuffer(IndexedVertexBuffer& indexedBufferMemory, const ML::List<uint32_t>& indices) {
    indexedBufferMemory.indexBufferLength = indices.getSize();
    VkDeviceSize bufferSize = indexedBufferMemory.indexBufferLength * sizeof(uint32_t);
    BufferMemory stagingBuffer{};

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);
    memcpy(data, indices.getData(), bufferSize);
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexedBufferMemory.indexBufferID, indexedBufferMemory.indexBufferMemoryID);

    CopyBuffer(stagingBuffer.bufferID, indexedBufferMemory.indexBufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::BindMesh(IndexedVertexBuffer mesh) {
    VkBuffer vertexBuffers[] = { mesh.vertBufferID };
    VkDeviceSize offsets[] = { 0 };
    for (uint32_t i = 0; i < getNumSwapchains(); i++) {
        vkCmdBindVertexBuffers(primaryCommandBuffer.commandBuffers[i], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(primaryCommandBuffer.commandBuffers[i], mesh.indexBufferID, 0, VK_INDEX_TYPE_UINT32);
    }
}

void VulkanRenderer::DrawIndexed(IndexedVertexBuffer mesh) {
    for (uint32_t i = 0; i < numSwapchains; i++) {
        vkCmdDrawIndexed(primaryCommandBuffer.commandBuffers[i], static_cast<uint32_t>(mesh.indexBufferLength), 1, 0, 0, 0);
    }
}

void VulkanRenderer::DestroyIndexedMesh(IndexedVertexBuffer mesh){
    vkDestroyBuffer(device, mesh.vertBufferID, nullptr);
    vkFreeMemory(device, mesh.vertBufferMemoryID, nullptr);
    vkDestroyBuffer(device, mesh.indexBufferID, nullptr);
    vkFreeMemory(device, mesh.indexBufferMemoryID, nullptr);
}


