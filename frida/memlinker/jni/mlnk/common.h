#ifndef _MLNK_COMMON_H_
#define _MLNK_COMMON_H_

#include <utility>  // for std::move, std::forward

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
private:                     \
TypeName(const TypeName&);               \
TypeName& operator=(const TypeName&)

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
private:                     \
TypeName();                                    \
DISALLOW_COPY_AND_ASSIGN(TypeName)

#undef PAGE_SIZE

#define PAGE_SIZE 4096


#undef PAGE_MASK
#define PAGE_MASK (~(PAGE_SIZE - 1))

// Returns the address of the page containing address 'x'.
#define PAGE_START(x) ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE-1))


#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0


// ScopeGuard ensures that the specified functor is executed no matter how the
// current scope exits.
template <typename F>
class ScopeGuard {
 public:
  ScopeGuard(F&& f) : f_(std::forward<F>(f)), active_(true) {}

  ScopeGuard(ScopeGuard&& that) noexcept : f_(std::move(that.f_)), active_(that.active_) {
    that.active_ = false;
  }

  template <typename Functor>
  ScopeGuard(ScopeGuard<Functor>&& that) : f_(std::move(that.f_)), active_(that.active_) {
    that.active_ = false;
  }

  ~ScopeGuard() {
    if (active_) f_();
  }

  ScopeGuard() = delete;
  ScopeGuard(const ScopeGuard&) = delete;
  void operator=(const ScopeGuard&) = delete;
  void operator=(ScopeGuard&& that) = delete;

  void disable() { active_ = false; }

  bool active() const { return active_; }

 private:
  template <typename Functor>
  friend class ScopeGuard;

  F f_;
  bool active_;
};

template <typename F>
ScopeGuard<F> make_scope_guard(F&& f) {
  return ScopeGuard<F>(std::forward<F>(f));
}

/**
 * Returns true if the binary representation of the argument is all zeros
 * or has exactly one bit set. Contrary to the macro name, this macro
 * DOES NOT determine if the provided value is a power of 2. In particular,
 * this function falsely returns true for powerof2(0) and some negative
 * numbers.
 */
#define powerof2(x)                                               \
  ({                                                              \
    __typeof__(x) _x = (x);                                       \
    __typeof__(x) _x2;                                            \
    __builtin_add_overflow(_x, -1, &_x2) ? 1 : ((_x2 & _x) == 0); \
  })
  
#endif