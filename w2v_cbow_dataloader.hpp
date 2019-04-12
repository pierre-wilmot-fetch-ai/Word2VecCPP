#pragma once
//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "tensor.hpp"
#include "dataloader.hpp"

#include <exception>
#include <fstream>
#include <map>
#include <string>
#include <utility>

namespace fetch {
namespace ml {

template <typename T>
class CBOWLoader : public DataLoader<fetch::math::Tensor<T>, uint64_t>
{
public:
  CBOWLoader(uint64_t window_size)
    : currentSentence_(0)
    , currentWord_(0)
    , window_size_(window_size)
  {}

  virtual uint64_t Size() const
  {
    uint64_t size(0);
    for (auto const &s : data_)
    {
      if ((uint64_t)s.size() > (2 * window_size_))
      {
        size += (uint64_t)s.size() - (2 * window_size_);
      }
    }
    return size;
  }

  virtual bool IsDone() const
  {
    if (data_.empty())
    {
      return true;
    }
    if (currentSentence_ >= data_.size())
    {
      return true;
    }
    else if (currentSentence_ >= data_.size() - 1)  // In the last sentence
    {
      if (currentWord_ > data_.at(currentSentence_).size() - (2 * window_size_ + 1))
      {
        return true;
      }
    }
    return false;
  }

  virtual void Reset()
  {
    std::random_shuffle(data_.begin(), data_.end());
    currentSentence_ = 0;
    currentWord_     = 0;
  }

  /*
   * Advance the cursor by offset
   * Used to train on different part of the dataset in a multithreaded environment
   */
  void SetOffset(unsigned int offset)
  {
    offset = offset % Size();
    while (offset > data_[currentSentence_].size())
      {
	offset -= data_[currentSentence_].size();
	currentSentence_++;
      }
    if (offset < data_[currentSentence_].size() - window_size_)
      {
	currentWord_ = offset;
      }
    else
      {
	currentSentence_++;
	currentWord_ = 0;
      }
  }
  
  /*
   * Remove words that appears less than MIN times
   * This is a destructive operation
   */
  void RemoveInfrequent(unsigned int min)
  {
    // Removing words while keeping indexes consecutive takes too long
    // So creating a new object, not the most efficient, but good enought for now
    CBOWLoader new_loader(window_size_);
    std::map<uint64_t, std::pair<std::string, uint64_t>> reverse_vocab;
    for (auto const &kvp : vocab_)
      {
	reverse_vocab[kvp.second.first] = std::make_pair(kvp.first, kvp.second.second);
      }
    for (auto const & sentence : data_)
      {
	std::string s;
	for (auto const & word : sentence)
	  {
	    if (reverse_vocab[word].second >= min)
	      {
		s += reverse_vocab[word].first + " ";
	      }
	  }
	new_loader.AddData(s);
      }
    data_ = std::move(new_loader.data_);
    vocab_ = std::move(new_loader.vocab_);
  }

  virtual std::pair<fetch::math::Tensor<T>, uint64_t> GetNext()
  {
    fetch::math::Tensor<T> t(window_size_ * 2);
    uint64_t               label = data_[currentSentence_][currentWord_ + window_size_];
    for (uint64_t i(0); i < window_size_; ++i)
    {
      t.At(i)                = T(data_[currentSentence_][currentWord_ + i]);
      t.At(i + window_size_) = T(data_[currentSentence_][currentWord_ + window_size_ + i + 1]);
    }
    currentWord_++;
    if (currentWord_ >= data_.at(currentSentence_).size() - (2 * window_size_))
    {
      currentWord_ = 0;
      currentSentence_++;
    }
    return std::make_pair(t, label);
  }

  std::size_t VocabSize() const
  {
    return vocab_.size();
  }

  bool AddData(std::string const &s)
  {
    std::vector<uint64_t> indexes = StringsToIndexes(PreprocessString(s));
    if (indexes.size() >= 2 * window_size_ + 1)
    {
      data_.push_back(std::move(indexes));
      return true;
    }
    return false;
  }

  std::map<std::string, std::pair<uint64_t, uint64_t>> const &GetVocab() const
  {
    return vocab_;
  }

  std::string WordFromIndex(uint64_t index)
  {
    for (auto const &kvp : vocab_)
      {
	if (kvp.second.first == index)
	  {
	    return kvp.first;
	  }
      }
    return "";
  }

private:
  std::vector<uint64_t> StringsToIndexes(std::vector<std::string> const &strings)
  {
    std::vector<uint64_t> indexes;
    if (strings.size() >= 2 * window_size_ + 1)  // Don't bother processing too short inputs
    {
      indexes.reserve(strings.size());
      for (std::string const &s : strings)
      {
        auto value = vocab_.insert(std::make_pair(s, std::make_pair((uint64_t)(vocab_.size()), 0)));
        indexes.push_back((*value.first).second.first);
	value.first->second.second++;
      }
    }
    return indexes;
  }

  std::vector<std::string> PreprocessString(std::string const &s)
  {
    std::string result;
    result.reserve(s.size());
    for (auto const &c : s)
    {
      result.push_back(std::isalpha(c) ? (char)std::tolower(c) : ' ');
    }

    std::string              word;
    std::vector<std::string> words;
    for (std::stringstream ss(result); ss >> word;)
    {
      words.push_back(word);
    }
    return words;
  }

private:
  uint64_t                                                currentSentence_;
  uint64_t                                                currentWord_;
  uint64_t                                                window_size_;
  std::map<std::string, std::pair<uint64_t, uint64_t>>    vocab_;
  std::vector<std::vector<uint64_t>>                      data_;
};
}  // namespace ml
}  // namespace fetch
