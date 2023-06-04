/*
 * nnoremap <F5> :!clang++ % -O3 -g -std=c++17 -o %:r && ./%:r <CR>
 *
 *
 * python
 * # input
 * #  P: 数组
 * #  N: 最大错误数
 * #  T: 分界点
 * # output:
 * #  (正确率, 最大压缩比)
 * > Est = lambda P,N,T: (N/(prod([N/i for i in P[:-T]]) * (N ** T - N) + N), sum(P) / prod(P[-T:]))
 * # input
 * #  C: 最大输入长度
 * # output:
 * #  (正确率，压缩比，合法)
 * > Est = lambda P,N,C,T: (N/(prod([N/i for i in P[:-T]]) * (C/prod(P[-T:])) * (N ** T - N) + N), sum(P) / C, C < prod(P[-T:]))
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <new>
constexpr int arr1[] = {113, 127, 131, 137, 139, 149, 151};
constexpr int arr2[] = {157, 163, 167};
constexpr int arr3[] = {3348183, 3382251, 1816961};
template <size_t Size>
constexpr int sum(const int (&arr)[Size])
{
    int ret = 0;
    for (int i = 0; i < Size; ++i)
        ret += arr[i];
    return ret;
}
template <size_t Size>
constexpr int64_t prod(const int (&arr)[Size])
{
  int64_t ret = 1;
  for (int i = 0; i < Size; ++i) ret *= arr[i];
  return ret;
}
template <size_t Size>
constexpr size_t countof(const int (&arr)[Size])
{
  return Size;
}

struct FixedStack{
  int* cur;
  int* end;
  int buf[1];
  inline void push(int idx){
    if (cur == end) return;
    *cur++ = idx;
  }
  FixedStack(int n): cur(buf), end(buf + n) {}
};

template<typename T>
struct Ctx {
  T* _begin;
  T* _end;
  inline void update(T (&sig)[sum(arr1) + sum(arr2)], int idx, T chksum){
    int s = 0;
    for (auto p: arr1){
      sig[s + idx % p] ^= chksum;
      s += p;
    }
    for (auto p: arr2){
      sig[s + idx % p] ^= chksum;
      s += p;
    }
  }
  Ctx(T* begin, T* end):_begin(begin),_end(end){

  }
  constexpr size_t size() const { return sum(arr1) + sum(arr2); }
  void fetch(T (&sig)[sum(arr1) + sum(arr2)]){
    for (T* p = _begin; p < _end; ++p){
      auto idx = p - _begin;
      update(sig, idx, *p);
    }
  }

  template<int S, int N>
  inline int F(T (&A)[sum(arr1) + sum(arr2)], T (&B)[sum(arr1) + sum(arr2)], int64_t v, FixedStack* stk){
    int ret = 0;
    for (auto i = S; i < S + arr2[N]; ++i){
      if (A[i] != B[i]){
        // printf("d:%d, %d\n", i, i-S);
        ret += F<S+arr2[N], N+1>(A, B, v + arr3[N]*(i-S), stk);
      }
    }
    return ret;
  }
  template<>
  inline int F<sum(arr1) + sum(arr2), countof(arr2)>(T (&A)[sum(arr1) + sum(arr2)], T (&B)[sum(arr1) + sum(arr2)], int64_t v, FixedStack* stk){
    v %= prod(arr2);
    // printf("v %ld\n", v);
    if (v < _end - _begin){
      int s = 0;
      for(auto p: arr1){
        if (A[s + v%p] == B[s + v % p]) return 0;
        s += p;
      }
      if (stk) stk->push(v);
      // printf("%ld\n", v);
      return 1;
    }
    return 0;
  }
  inline int cmp(T (&A)[sum(arr1) + sum(arr2)], T (&B)[sum(arr1) + sum(arr2)], FixedStack* r)
  {
    return F<sum(arr1), 0>(A, B, 0, r);
  }
};



int main()
{
  short t[1024];

  Ctx<short> c1(t, t + 1024);
  short sig[c1.size()] = {0};
  short cur[c1.size()] = {0};
  c1.fetch(sig);
  // for(auto i: sig) printf("%d ", i);
  // printf("\n");
  t[0] = 100;
  t[4] = 89;
  t[17] = 1;
  c1.fetch(cur);
  // for(auto i: cur) printf("%d ", i);
  // printf("\n");
  char _back[100 * 4 + 16];
  FixedStack* r = new (_back) FixedStack(100);
  c1.cmp(sig, cur, r);
  return r->cur - r->buf;
}
