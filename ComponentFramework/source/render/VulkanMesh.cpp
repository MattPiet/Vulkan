#include "render/VulkanRenderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "core/tiny_obj_loader.h"

IndexedVertexBuffer VulkanRenderer::LoadModelIndexed(const char* filename) {
    IndexedVertexBuffer indexedVertexBuffer{};
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

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


            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
    CreateVertexBuffer(indexedVertexBuffer, vertices);
    CreateIndexBuffer(indexedVertexBuffer, indices);
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