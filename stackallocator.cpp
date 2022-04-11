#include <iostream>
#include <memory>
#include <type_traits>
//List
template<size_t N>
class alignas(::max_align_t) StackStorage {
 public:
  StackStorage() = default;

  StackStorage(const StackStorage&) = delete;

  StackStorage& operator=(const StackStorage&) = delete;

  uint8_t* allocate(size_t count, size_t alignment) {
    position_ += (alignment - position_ % alignment) % alignment;
    uint8_t* result = buffer_ + position_;
    position_ += count;
    return result;
  }

 private:
  uint8_t buffer_[N];
  size_t position_ = 0;
};

template<typename Type, size_t N>
class StackAllocator {
 public:
  using value_type = Type;
  template<typename Node>
  struct rebind { using other = StackAllocator<Node, N>; };

  StackAllocator() = delete;

  StackAllocator(StackStorage<N>& other) : store_(&other) {}

  template<typename OtherType>
  StackAllocator(const StackAllocator<OtherType, N>& other)
      :store_(other.store_) {}

  StackAllocator& operator=(const StackAllocator& other) = default;

  Type* allocate(size_t count) {
    return reinterpret_cast<Type*>(store_->allocate(count * sizeof(Type),
                                                    alignof(Type)));
  }

  void deallocate(Type* pointer, size_t count) {
    if(pointer || count){

    }
  }

 private:
  template<typename AllType, size_t AllN>
  friend class StackAllocator;
  StackStorage<N>* store_;
};

template<typename Type, typename Allocator = std::allocator<Type> >
class List {
 private:
  struct BaseNode {
    BaseNode() = default;

    BaseNode* next_ = nullptr;
    BaseNode* prev_ = nullptr;
  };
  struct Node : BaseNode {
    Node() = default;

    explicit Node(const Type& value) : value_(value) {}

    Type value_;
  };

 public:
  template<bool isConst>
  class common_iterator {
   public:
    friend class List;
    using difference_type = std::ptrdiff_t;;
    using iterator_category = std::bidirectional_iterator_tag;
    using pointer = std::conditional_t<isConst, const Type*, Type*>;
    using reference = std::conditional_t<isConst, const Type&, Type&>;
    using value_type = std::conditional_t<isConst, const Type, Type>;

    explicit common_iterator(BaseNode* it) : it_(it) {}

    common_iterator& operator++() {
      it_ = it_->next_;
      return *this;
    }

    common_iterator operator++(int) {
      common_iterator copy = *this;
      ++(*this);
      return copy;
    }

    common_iterator& operator--() {
      it_ = it_->prev_;
      return *this;
    }

    common_iterator operator--(int) {
      common_iterator copy = *this;
      --(*this);
      return copy;
    }

    bool operator==(const common_iterator<isConst>& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const common_iterator<isConst>& other) const {
      return it_ != other.it_;
    }

    reference operator*() const {
      return static_cast<std::conditional_t<isConst,
                                            const Node*,
                                            Node*> >(it_)->value_;
    }

    pointer operator->() const {
      return &static_cast<std::conditional_t<isConst,
                                             const Node*,
                                             Node*> >(it_)->value_;
    }

    operator common_iterator<true>() {
      return common_iterator<true>(it_);
    }

   private:
    BaseNode* it_;
  };
  using iterator = common_iterator<false>;
  using const_iterator = common_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() {
    return iterator(fake_node_->next_);
  };

  const_iterator begin() const {
    return iterator(fake_node_->next_);
  };

  const_iterator cbegin() const {
    return const_iterator(fake_node_->next_);
  };

  iterator end() {
    return iterator(fake_node_);
  };

  const_iterator end() const {
    return const_iterator(fake_node_);
  };

  const_iterator cend() const {
    return const_iterator(fake_node_);
  };

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  };

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  };

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  };

  reverse_iterator rend() {
    return reverse_iterator(begin());
  };

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  };

  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  };

  using NodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<
      Node>;
  using BaseNodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<
      BaseNode>;
  using NodeAllocTraits = std::allocator_traits<NodeAlloc>;
  using BaseNodeAllocTraits = std::allocator_traits<BaseNodeAlloc>;

  List(const Allocator& alloc = Allocator())
      : alloc_(alloc), alloc_fake_(alloc) {
    construct_fake_node();
  }

  List(size_t count,
       const Type& value,
       const Allocator& alloc = Allocator())
      : alloc_(alloc), alloc_fake_(alloc) {
    construct_fake_node();
    try {
      while (count-- != 0) {
        construct_node(end(), value);
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List(size_t count, const Allocator& alloc = Allocator()) : alloc_(alloc), alloc_fake_(alloc) {
    construct_fake_node();
    try {
      while (count-- != 0) {
        construct_node(end());
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List(const List& other)
      : alloc_(std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc_)),
        alloc_fake_(alloc_) {
    construct_fake_node();
    try {
      for (auto it = other.begin(); it != other.end(); ++it) {
        construct_node(end(), *it);
      }
    } catch (...) {
      delete_list();
      throw;
    }
  }

  List& operator=(const List& other) {
    List copy(std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value ? other.alloc_ : alloc_);
    for (auto it = other.begin(); it != other.end(); ++it) {
      copy.push_back(*it);
    }
    swap(copy);
    return *this;
  }

  Allocator get_allocator() { return alloc_; }

  size_t size() const { return size_; }

  void push_back(const Type& value) {
    construct_node(end(), value);
  }

  void push_front(const Type& value) {
    construct_node(begin(), value);
  }

  void pop_back() {
    erase(--end());
  }

  void pop_front() {
    erase(begin());
  }

  void insert(const const_iterator& it, const Type& elem) {
    construct_node(it, elem);
  }

  void erase(const const_iterator& it) {
    BaseNode* node = it.it_;
    node->prev_->next_ = node->next_;
    node->next_->prev_ = node->prev_;
    delete_node(reinterpret_cast<Node*>(node));
    --size_;
  }

  ~List() {
    delete_list();
  }

 private:
  void swap(List& other) {
    if (this != &other) {
      std::swap(size_, other.size_);
      std::swap(fake_node_, other.fake_node_);
      std::swap(alloc_, other.alloc_);
      std::swap(alloc_fake_, other.alloc_fake_);
    }
  }

  void delete_node(Node* node) {
    NodeAllocTraits::destroy(alloc_, node);
    NodeAllocTraits::deallocate(alloc_, node, 1);
  }

  void delete_list() {
    iterator it = begin();
    while (size_-- != 0) {
      iterator current = it++;
      delete_node(reinterpret_cast<Node*>(current.it_));
    }
    BaseNodeAllocTraits::destroy(alloc_fake_, fake_node_);
    BaseNodeAllocTraits::deallocate(alloc_fake_, fake_node_, 1);
    fake_node_ = nullptr;
  }

  void construct_fake_node() {
    fake_node_ = BaseNodeAllocTraits::allocate(alloc_fake_, 1);
    BaseNodeAllocTraits::construct(alloc_fake_, fake_node_);
    fake_node_->next_ = fake_node_;
    fake_node_->prev_ = fake_node_;
  }

  void construct_node(const const_iterator& iter, const Type& value) {
    Node* node = NodeAllocTraits::allocate(alloc_, 1);
    try {
      NodeAllocTraits::construct(alloc_, node, value);
    } catch (...) {
      NodeAllocTraits::deallocate(alloc_, node, 1);
      throw;
    }
    construct_node_pointers(iter, node);
  }

  void construct_node(const const_iterator& iter) {
    Node* node = NodeAllocTraits::allocate(alloc_, 1);
    try {
      NodeAllocTraits::construct(alloc_, node);
    } catch (...) {
      NodeAllocTraits::deallocate(alloc_, node, 1);
      throw;
    }
    construct_node_pointers(iter, node);
  }

  void construct_node_pointers(const const_iterator& iter, Node* node) {
    BaseNode* pointer = iter.it_;
    BaseNode* pointer_prev = iter.it_->prev_;
    pointer->prev_ = node;
    pointer_prev->next_ = node;
    node->prev_ = pointer_prev;
    node->next_ = pointer;
    ++size_;
  }

  size_t size_ = 0;
  BaseNode* fake_node_ = nullptr;
  NodeAlloc alloc_;
  BaseNodeAlloc alloc_fake_;
};

