// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/huffman/huffman_builder.h"

#include <algorithm>

#include "base/logging.h"

namespace net {

namespace transport_security_state {

namespace {

class HuffmanNode {
 public:
  HuffmanNode(uint8_t value,
              uint32_t count,
              std::unique_ptr<HuffmanNode> left,
              std::unique_ptr<HuffmanNode> right)
      : value_(value),
        count_(count),
        left_(std::move(left)),
        right_(std::move(right)) {}
  ~HuffmanNode() {}

  bool IsLeaf() const {
    return left_.get() == nullptr && right_.get() == nullptr;
  }

  uint8_t value() const { return value_; }
  uint32_t count() const { return count_; }
  const std::unique_ptr<HuffmanNode>& left() const { return left_; }
  const std::unique_ptr<HuffmanNode>& right() const { return right_; }

 private:
  uint8_t value_;
  uint32_t count_;
  std::unique_ptr<HuffmanNode> left_;
  std::unique_ptr<HuffmanNode> right_;
};

bool CompareNodes(const std::unique_ptr<HuffmanNode>& lhs,
                  const std::unique_ptr<HuffmanNode>& rhs) {
  return lhs->count() < rhs->count();
}

}  // namespace

HuffmanBuilder::HuffmanBuilder() {}

HuffmanBuilder::~HuffmanBuilder() {}

void HuffmanBuilder::RecordUsage(uint8_t character) {
  counts_[character] += 1;
}

HuffmanRepresentationTable HuffmanBuilder::ToTable() {
  HuffmanRepresentationTable table;
  std::unique_ptr<HuffmanNode> node(BuildTree());

  TreeToTable(node.get(), 0, 0, &table);
  return table;
}

void HuffmanBuilder::TreeToTable(HuffmanNode* node,
                                 uint32_t bits,
                                 uint32_t number_of_bits,
                                 HuffmanRepresentationTable* table) {
  if (node->IsLeaf()) {
    HuffmanRepresentation item;
    item.bits = bits;
    item.number_of_bits = number_of_bits;

    table->insert(HuffmanRepresentationPair(node->value(), item));
  } else {
    uint32_t new_bits = bits << 1;
    TreeToTable(node->left().get(), new_bits, number_of_bits + 1, table);
    TreeToTable(node->right().get(), new_bits | 1, number_of_bits + 1, table);
  }
}

std::vector<uint8_t> HuffmanBuilder::ToVector() {
  std::vector<uint8_t> bytes;
  std::unique_ptr<HuffmanNode> node(BuildTree());
  WriteToVector(node.get(), &bytes);
  return bytes;
}

uint32_t HuffmanBuilder::WriteToVector(HuffmanNode* node,
                                       std::vector<uint8_t>* vector) {
  uint8_t left_value;
  uint8_t right_value;
  uint32_t child_position;

  if (node->left()->IsLeaf()) {
    left_value = 128 | node->left()->value();
  } else {
    child_position = WriteToVector(node->left().get(), vector);
    CHECK(child_position < 512) << "huffman tree too large";
    left_value = child_position / 2;
  }

  if (node->right()->IsLeaf()) {
    right_value = 128 | node->right()->value();
  } else {
    child_position = WriteToVector(node->right().get(), vector);
    CHECK(child_position < 512) << "huffman tree to large";
    right_value = child_position / 2;
  }

  uint32_t position = static_cast<uint32_t>(vector->size());
  vector->push_back(left_value);
  vector->push_back(right_value);
  return position;
}

std::unique_ptr<HuffmanNode> HuffmanBuilder::BuildTree() {
  std::vector<std::unique_ptr<HuffmanNode>> nodes;
  nodes.reserve(counts_.size());

  for (const auto& item : counts_) {
    if (item.second > 0) {
      std::unique_ptr<HuffmanNode> node(
          new HuffmanNode(item.first, item.second, nullptr, nullptr));
      nodes.push_back(std::move(node));
    }
  }

  if (nodes.size() < 2) {
    return std::move(nodes[0]);
  }

  std::stable_sort(nodes.begin(), nodes.end(), CompareNodes);

  while (nodes.size() > 1) {
    std::unique_ptr<HuffmanNode> a = std::move(nodes[0]);
    std::unique_ptr<HuffmanNode> b = std::move(nodes[1]);

    uint32_t count_a = a->count();
    uint32_t count_b = b->count();

    std::unique_ptr<HuffmanNode> parent(
        new HuffmanNode(0, count_a + count_b, std::move(a), std::move(b)));

    nodes.erase(nodes.begin());
    nodes[0] = std::move(parent);

    std::stable_sort(nodes.begin(), nodes.end(), CompareNodes);
  }

  return std::move(nodes[0]);
}

}  // namespace transport_security_state

}  // namespace net
