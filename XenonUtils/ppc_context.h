#ifndef PPC_CONTEXT_H_INCLUDED
#define PPC_CONTEXT_H_INCLUDED

#ifndef PPC_CONFIG_H_INCLUDED
#error "ppc_config.h must be included before ppc_context.h"
#endif

#include <climits>
#include <chrono>
#include <cmath>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <x86/avx.h>
#include <x86/sse.h>
#include <x86/sse4.1.h>

inline constexpr uint64_t PPC_TIME_BASE_FREQUENCY = 50'000'000ULL;
inline constexpr uint64_t PPC_TIME_BASE_NANOSECONDS_PER_TICK = 20ULL;

constexpr uint64_t PPC_TIME_BASE_FROM_NANOSECONDS(uint64_t nanoseconds) noexcept
{
    return nanoseconds / PPC_TIME_BASE_NANOSECONDS_PER_TICK;
}

inline uint64_t PPC_READ_TIME_BASE() noexcept
{
    using namespace std::chrono;
    const auto nanoseconds = duration_cast<std::chrono::nanoseconds>(
        steady_clock::now().time_since_epoch()).count();
    return PPC_TIME_BASE_FROM_NANOSECONDS(static_cast<uint64_t>(nanoseconds));
}

// SSE3 constants are missing from simde
#ifndef _MM_DENORMALS_ZERO_MASK
#define _MM_DENORMALS_ZERO_MASK 0x0040
#endif

#define PPC_JOIN(x, y) x##y
#define PPC_XSTRINGIFY(x) #x
#define PPC_STRINGIFY(x) PPC_XSTRINGIFY(x)
#define PPC_FUNC(x) void x(PPCContext& __restrict ctx, uint8_t* base)
#define PPC_FUNC_IMPL(x) extern "C" PPC_FUNC(x)
#define PPC_EXTERN_FUNC(x) extern PPC_FUNC(x)
#define PPC_WEAK_FUNC(x) __attribute__((weak,noinline)) PPC_FUNC(x)

#define PPC_FUNC_PROLOGUE() __builtin_assume(((size_t)base & 0x1F) == 0)

#ifndef PPC_LOAD_U8
#define PPC_LOAD_U8(x) *(volatile uint8_t*)(base + (x))
#endif

#ifndef PPC_LOAD_U16
#define PPC_LOAD_U16(x) __builtin_bswap16(*(volatile uint16_t*)(base + (x)))
#endif

#ifndef PPC_LOAD_U32
#define PPC_LOAD_U32(x) __builtin_bswap32(*(volatile uint32_t*)(base + (x)))
#endif

#ifndef PPC_LOAD_U64
#define PPC_LOAD_U64(x) __builtin_bswap64(*(volatile uint64_t*)(base + (x)))
#endif

// TODO: Implement.
// These are currently unused. However, MMIO loads could possibly be handled statically with some profiling and a fallback.
// The fallback would be a runtime exception handler which will intercept reads from MMIO regions 
// and log the PC for compiling to static code later.
#ifndef PPC_MM_LOAD_U8
#define PPC_MM_LOAD_U8(x)  PPC_LOAD_U8 (x)
#endif

#ifndef PPC_MM_LOAD_U16
#define PPC_MM_LOAD_U16(x) PPC_LOAD_U16(x)
#endif

#ifndef PPC_MM_LOAD_U32
#define PPC_MM_LOAD_U32(x) PPC_LOAD_U32(x)
#endif

#ifndef PPC_MM_LOAD_U64
#define PPC_MM_LOAD_U64(x) PPC_LOAD_U64(x)
#endif

#ifndef PPC_STORE_U8
#define PPC_STORE_U8(x, y) *(volatile uint8_t*)(base + (x)) = (y)
#endif

#ifndef PPC_STORE_U16
#define PPC_STORE_U16(x, y) *(volatile uint16_t*)(base + (x)) = __builtin_bswap16(y)
#endif

#ifndef PPC_STORE_U32
#define PPC_STORE_U32(x, y) *(volatile uint32_t*)(base + (x)) = __builtin_bswap32(y)
#endif

#ifndef PPC_STORE_U64
#define PPC_STORE_U64(x, y) *(volatile uint64_t*)(base + (x)) = __builtin_bswap64(y)
#endif

// Keep full-vector guest stores overridable for runtimes that maintain
// CPU/GPU mirrors of physical memory. The value supplied by the recompiler is
// already byte-shuffled into guest memory order.
#ifndef PPC_STORE_V128
#define PPC_STORE_V128(x, y) simde_mm_store_si128((simde__m128i*)(base + (x)), (y))
#endif

// MMIO Store handling is completely reliant on being preeceded by eieio.
// TODO: Verify if that's always the case.
#ifndef PPC_MM_STORE_U8
#define PPC_MM_STORE_U8(x, y)   PPC_STORE_U8 (x, y)
#endif

#ifndef PPC_MM_STORE_U16
#define PPC_MM_STORE_U16(x, y)  PPC_STORE_U16(x, y)
#endif

#ifndef PPC_MM_STORE_U32
#define PPC_MM_STORE_U32(x, y)  PPC_STORE_U32(x, y)
#endif

#ifndef PPC_MM_STORE_U64
#define PPC_MM_STORE_U64(x, y)  PPC_STORE_U64(x, y)
#endif

#ifndef PPC_CALL_FUNC
#define PPC_CALL_FUNC(x) x(ctx, base)
#endif

// Runtime diagnostics may override this at compile time.  The default is
// deliberately a no-op so generated code remains standalone and behaviour is
// unchanged outside the title runtime.
#ifndef PPC_RUNTIME_FUNCTION_ENTER
#define PPC_RUNTIME_FUNCTION_ENTER(address, context, imageBase) ((void)0)
#endif

#ifndef PPC_RUNTIME_MEMORY_READ
#define PPC_RUNTIME_MEMORY_READ(instruction, address, value) ((void)0)
#endif

#ifndef PPC_RUNTIME_PC_PROBE
#define PPC_RUNTIME_PC_PROBE(address, context, imageBase) ((void)0)
#endif

// Xenon-specific 16-cycle delay hint. It has no architectural side effects,
// so standalone generated code may leave it empty. Embedding runtimes can
// override this to provide a host spin hint and cooperative stop observation.
#ifndef PPC_RUNTIME_DB16CYC
#define PPC_RUNTIME_DB16CYC() ((void)0)
#endif

#define PPC_MEMORY_SIZE 0x100000000ull

#define PPC_LOOKUP_FUNC(x, y) *(PPCFunc**)(x + PPC_IMAGE_BASE + PPC_IMAGE_SIZE + (uint64_t(uint32_t(y) - PPC_CODE_BASE) * 2))

#ifndef PPC_RUNTIME_INDIRECT_CALL
#define PPC_RUNTIME_INDIRECT_CALL(address, context, imageBase) \
    (PPC_LOOKUP_FUNC(imageBase, address))(context, imageBase)
#endif

#ifndef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(x) PPC_RUNTIME_INDIRECT_CALL(x, ctx, base)
#endif

typedef void PPCFunc(struct PPCContext& __restrict__ ctx, uint8_t* base);

struct PPCFuncMapping
{
    size_t guest;
    PPCFunc* host;
};

extern PPCFuncMapping PPCFuncMappings[];

union PPCRegister
{
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
    float f32;
    double f64;
};

// High 64 bits of a 64x64 unsigned product, expressed entirely in portable
// 32-bit partial products.  This is used to derive PPC64 mulhd without
// relying on a compiler-specific 128-bit integer type in generated code.
inline uint64_t PPC_MULHDU(uint64_t a, uint64_t b) noexcept
{
    const uint64_t a0 = static_cast<uint32_t>(a);
    const uint64_t a1 = a >> 32;
    const uint64_t b0 = static_cast<uint32_t>(b);
    const uint64_t b1 = b >> 32;
    const uint64_t p00 = a0 * b0;
    const uint64_t p01 = a0 * b1;
    const uint64_t p10 = a1 * b0;
    const uint64_t p11 = a1 * b1;
    const uint64_t carry = (p00 >> 32) + static_cast<uint32_t>(p01) + static_cast<uint32_t>(p10);
    return p11 + (p01 >> 32) + (p10 >> 32) + (carry >> 32);
}

// PowerPC mulhd returns the high signed doubleword.  The adjustment converts
// the unsigned 128-bit product to its two's-complement signed equivalent.
inline uint64_t PPC_MULHD(uint64_t a, uint64_t b) noexcept
{
    uint64_t high = PPC_MULHDU(a, b);
    if (static_cast<int64_t>(a) < 0) high -= b;
    if (static_cast<int64_t>(b) < 0) high -= a;
    return high;
}

struct PPCXERRegister
{
    uint8_t so;
    uint8_t ov;
    uint8_t ca;
};

struct PPCCRRegister
{
    uint8_t lt;
    uint8_t gt;
    uint8_t eq;
    union
    {
        uint8_t so;
        uint8_t un;
    };

    template<typename T>
    inline void compare(T left, T right, const PPCXERRegister& xer) noexcept
    {
        lt = left < right;
        gt = left > right;
        eq = left == right;
        so = xer.so;
    }

    inline void compare(double left, double right) noexcept
    {
        un = __builtin_isnan(left) || __builtin_isnan(right);
        lt = !un && (left < right);
        gt = !un && (left > right);
        eq = !un && (left == right);
    }

    inline void setFromMask(simde__m128 mask, int imm) noexcept
    {
        int m = simde_mm_movemask_ps(mask);
        lt = m == imm; // all equal
        gt = 0;
        eq = m == 0; // none equal
        so = 0;
    }

    inline void setFromMask(simde__m128i mask, int imm) noexcept
    {
        int m = simde_mm_movemask_epi8(mask);
        lt = m == imm; // all equal
        gt = 0;
        eq = m == 0; // none equal
        so = 0;
    }
};

union alignas(0x10) PPCVRegister
{
    int8_t s8[16];
    uint8_t u8[16];
    int16_t s16[8];
    uint16_t u16[8];
    int32_t s32[4];
    uint32_t u32[4];
    int64_t s64[2];
    uint64_t u64[2];
    float f32[4];
    double f64[2];
};

// VMX vslh: shift each 16-bit element of vA left by the low four bits of
// the corresponding vB element.  Compute into a temporary so vD may alias
// either source register, as the architectural instruction permits.
inline void PPC_VSLH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++)
    {
        const uint32_t shift = b.u16[i] & 0xF;
        result.u16[i] = static_cast<uint16_t>(static_cast<uint32_t>(a.u16[i]) << shift);
    }
    d = result;
}

inline void PPC_VSRAH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++)
    {
        const uint32_t shift = b.u16[i] & 0xF;
        const uint16_t logical = a.u16[i] >> shift;
        const uint16_t signFill = (a.u16[i] & 0x8000) && shift
            ? static_cast<uint16_t>(0xFFFFu << (16 - shift)) : 0;
        result.u16[i] = logical | signFill;
    }
    d = result;
}

inline void PPC_VSPLTISH(PPCVRegister& d, int32_t immediate) noexcept
{
    const uint16_t value = static_cast<uint16_t>(static_cast<int16_t>(immediate));
    for (size_t i = 0; i < 8; i++) d.u16[i] = value;
}

// VMX vandc: vA & ~vB, independently for all 128 bits.
inline void PPC_VANDC(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 16; i++) result.u8[i] = a.u8[i] & static_cast<uint8_t>(~b.u8[i]);
    d = result;
}

// VMX vmaxsh/vminsh compare signed 16-bit elements.  The temporary preserves
// architectural behavior when vD aliases either input.
inline void PPC_VMAXSH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++) result.s16[i] = a.s16[i] >= b.s16[i] ? a.s16[i] : b.s16[i];
    d = result;
}

inline void PPC_VMINSH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++) result.s16[i] = a.s16[i] < b.s16[i] ? a.s16[i] : b.s16[i];
    d = result;
}

// VSCR bit numbering is architectural (bit 31 is the least-significant bit
// of the right-aligned 32-bit status value). Saturating VMX operations set it
// persistently; only mtvscr can clear it.
constexpr uint32_t PPC_VSCR_SAT = 0x00000001u;

inline void PPC_VSUBSHS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    for (size_t i = 0; i < 8; i++)
    {
        const int32_t value = static_cast<int32_t>(a.s16[i]) - static_cast<int32_t>(b.s16[i]);
        if (value > INT16_MAX) { result.s16[i] = INT16_MAX; saturated = true; }
        else if (value < INT16_MIN) { result.s16[i] = INT16_MIN; saturated = true; }
        else result.s16[i] = static_cast<int16_t>(value);
    }
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VPKSWSS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    const auto pack = [&saturated](int32_t value) noexcept -> int16_t {
        if (value > INT16_MAX) { saturated = true; return INT16_MAX; }
        if (value < INT16_MIN) { saturated = true; return INT16_MIN; }
        return static_cast<int16_t>(value);
    };
    // Host lane order is the complete reversal of guest vector order.
    for (size_t i = 0; i < 4; i++) result.s16[i] = pack(b.s32[i]);
    for (size_t i = 0; i < 4; i++) result.s16[i + 4] = pack(a.s32[i]);
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VPKSWUS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    const auto pack = [&saturated](int32_t value) noexcept -> uint16_t {
        if (value > UINT16_MAX) { saturated = true; return UINT16_MAX; }
        if (value < 0) { saturated = true; return 0; }
        return static_cast<uint16_t>(value);
    };
    for (size_t i = 0; i < 4; i++) result.u16[i] = pack(b.s32[i]);
    for (size_t i = 0; i < 4; i++) result.u16[i + 4] = pack(a.s32[i]);
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

// VMX packing writes the vB lanes before vA in this host representation,
// because PPCVRegister is stored in reverse guest-vector lane order.
inline void PPC_VPKUWUM(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 4; i++) result.u16[i] = static_cast<uint16_t>(b.u32[i]);
    for (size_t i = 0; i < 4; i++) result.u16[i + 4] = static_cast<uint16_t>(a.u32[i]);
    d = result;
}

inline void PPC_VPKSHSS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    const auto pack = [&saturated](int16_t value) noexcept -> int8_t {
        if (value > INT8_MAX) { saturated = true; return INT8_MAX; }
        if (value < INT8_MIN) { saturated = true; return INT8_MIN; }
        return static_cast<int8_t>(value);
    };
    for (size_t i = 0; i < 8; i++) result.s8[i] = pack(b.s16[i]);
    for (size_t i = 0; i < 8; i++) result.s8[i + 8] = pack(a.s16[i]);
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VPKUHUS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    const auto pack = [&saturated](uint16_t value) noexcept -> uint8_t {
        if (value > UINT8_MAX) { saturated = true; return UINT8_MAX; }
        return static_cast<uint8_t>(value);
    };
    for (size_t i = 0; i < 8; i++) result.u8[i] = pack(b.u16[i]);
    for (size_t i = 0; i < 8; i++) result.u8[i + 8] = pack(a.u16[i]);
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VSUBUHS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    for (size_t i = 0; i < 8; i++)
    {
        if (a.u16[i] < b.u16[i]) { result.u16[i] = 0; saturated = true; }
        else result.u16[i] = static_cast<uint16_t>(a.u16[i] - b.u16[i]);
    }
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VRLH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++) {
        const uint32_t sh = b.u16[i] & 15;
        result.u16[i] = static_cast<uint16_t>((uint32_t(a.u16[i]) << sh) | (uint32_t(a.u16[i]) >> ((16 - sh) & 15)));
    }
    d = result;
}

inline void PPC_VSRH(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 8; i++) result.u16[i] = a.u16[i] >> (b.u16[i] & 15);
    d = result;
}

inline void PPC_VSRAB(PPCVRegister& d, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    for (size_t i = 0; i < 16; i++) {
        const uint32_t sh = b.u8[i] & 7;
        const uint8_t logical = a.u8[i] >> sh;
        const uint8_t fill = (a.u8[i] & 0x80) && sh ? static_cast<uint8_t>(0xFFu << (8 - sh)) : 0;
        result.u8[i] = logical | fill;
    }
    d = result;
}

inline void PPC_VADDSWS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    for (size_t i = 0; i < 4; i++) {
        const int64_t value = int64_t(a.s32[i]) + int64_t(b.s32[i]);
        if (value > INT32_MAX) { result.s32[i] = INT32_MAX; saturated = true; }
        else if (value < INT32_MIN) { result.s32[i] = INT32_MIN; saturated = true; }
        else result.s32[i] = static_cast<int32_t>(value);
    }
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

inline void PPC_VADDSBS(PPCVRegister& d, uint32_t& vscr, const PPCVRegister& a, const PPCVRegister& b) noexcept
{
    PPCVRegister result{};
    bool saturated = false;
    for (size_t i = 0; i < 16; i++) {
        const int32_t value = int32_t(a.s8[i]) + int32_t(b.s8[i]);
        if (value > INT8_MAX) { result.s8[i] = INT8_MAX; saturated = true; }
        else if (value < INT8_MIN) { result.s8[i] = INT8_MIN; saturated = true; }
        else result.s8[i] = static_cast<int8_t>(value);
    }
    if (saturated) vscr |= PPC_VSCR_SAT;
    d = result;
}

#define PPC_ROUND_NEAREST 0x00
#define PPC_ROUND_TOWARD_ZERO 0x01
#define PPC_ROUND_UP 0x02
#define PPC_ROUND_DOWN 0x03
#define PPC_ROUND_MASK 0x03

struct PPCFPSCRRegister
{
    // Host floating-point control state is kept separately from the guest
    // FPSCR.  The old representation used csr for both, which meant mffs could
    // only observe the two host-derived rounding bits and could not retain
    // architectural exception/result state.
    uint32_t csr{};
    uint32_t value{};

    static constexpr size_t HostToGuest[] = { PPC_ROUND_NEAREST, PPC_ROUND_DOWN, PPC_ROUND_UP, PPC_ROUND_TOWARD_ZERO };

    // simde does not handle denormal flags, so we need to implement per-arch.
#if defined(__x86_64__) || defined(_M_X64)
    static constexpr size_t RoundShift = 13;
    static constexpr size_t RoundMask = SIMDE_MM_ROUND_MASK;
    static constexpr size_t FlushMask = SIMDE_MM_FLUSH_ZERO_MASK | _MM_DENORMALS_ZERO_MASK;
    static constexpr size_t GuestToHost[] = { SIMDE_MM_ROUND_NEAREST, SIMDE_MM_ROUND_TOWARD_ZERO, SIMDE_MM_ROUND_UP, SIMDE_MM_ROUND_DOWN };

    inline uint32_t getcsr() noexcept
    {
        return simde_mm_getcsr();
    }

    inline void setcsr(uint32_t csr) noexcept
    {
        simde_mm_setcsr(csr);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    // RMode
    static constexpr size_t RoundShift = 22;
    static constexpr size_t RoundMask = 3 << RoundShift;
    // FZ and FZ16
    static constexpr size_t FlushMask = (1 << 19) | (1 << 24);
    // Nearest, Zero, -Infinity, -Infinity
    static constexpr size_t GuestToHost[] = { 0 << RoundShift, 3 << RoundShift, 1 << RoundShift, 2 << RoundShift };

    inline uint32_t getcsr() noexcept
    {
        uint64_t csr;
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(csr));
        return csr;
    }

    inline void setcsr(uint32_t csr) noexcept
    {
        __asm__ __volatile__("msr fpcr, %0" : : "r"(csr));
    }
#else
#   error "Missing implementation for FPSCR."
#endif

    inline uint32_t loadFromHost() noexcept
    {
        csr = getcsr();
        value &= ~PPC_ROUND_MASK;
        value |= HostToGuest[(csr & RoundMask) >> RoundShift];
        return value;
    }
        
    inline void storeFromGuest(uint32_t guestValue) noexcept
    {
        value = guestValue;
        csr &= ~RoundMask;
        csr |= GuestToHost[guestValue & PPC_ROUND_MASK];
        setcsr(csr);
    }

    inline void enableFlushModeUnconditional() noexcept
    {
        csr |= FlushMask;
        setcsr(csr);
    }

    inline void disableFlushModeUnconditional() noexcept
    {
        csr &= ~FlushMask;
        setcsr(csr);
    }

    inline void enableFlushMode() noexcept
    {
        if ((csr & FlushMask) != FlushMask) [[unlikely]]
        {
            csr |= FlushMask;
            setcsr(csr);
        }
    }

    inline void disableFlushMode() noexcept
    {
        if ((csr & FlushMask) != 0) [[unlikely]]
        {
            csr &= ~FlushMask;
            setcsr(csr);
        }
    }
};

// PowerPC FPSCR bit masks use the architecture's conventional MSB-first bit
// numbering.  These are the fields needed by frsqrte, including the summary
// bits copied to CR1 by its recording form.
inline constexpr uint32_t PPC_FPSCR_FX = 0x80000000u;
inline constexpr uint32_t PPC_FPSCR_FEX = 0x40000000u;
inline constexpr uint32_t PPC_FPSCR_VX = 0x20000000u;
inline constexpr uint32_t PPC_FPSCR_OX = 0x10000000u;
inline constexpr uint32_t PPC_FPSCR_ZX = 0x04000000u;
inline constexpr uint32_t PPC_FPSCR_VXSNAN = 0x01000000u;
inline constexpr uint32_t PPC_FPSCR_FR = 0x00040000u;
inline constexpr uint32_t PPC_FPSCR_FI = 0x00020000u;
inline constexpr uint32_t PPC_FPSCR_FPRF = 0x0001F000u;
inline constexpr uint32_t PPC_FPSCR_VXSQRT = 0x00000200u;
inline constexpr uint32_t PPC_FPSCR_VE = 0x00000080u;
inline constexpr uint32_t PPC_FPSCR_ZE = 0x00000010u;
inline constexpr uint32_t PPC_FPSCR_NI = 0x00000004u;

inline uint32_t PPC_FPRF_FROM_F64(uint64_t bits) noexcept
{
    constexpr uint64_t SignMask = 0x8000000000000000ull;
    constexpr uint64_t ExponentMask = 0x7FF0000000000000ull;
    constexpr uint64_t MantissaMask = 0x000FFFFFFFFFFFFFull;
    const bool negative = (bits & SignMask) != 0;
    const uint32_t exponent = static_cast<uint32_t>((bits & ExponentMask) >> 52);
    const uint64_t mantissa = bits & MantissaMask;

    // FPRF is C, FL, FG, FE, FU in bits 15..19 of FPSCR.  The encodings are
    // specified by the PowerPC result-class table and match the values used by
    // the pinned Xenia native instruction tests.
    uint32_t resultClass;
    if (exponent == 0x7FFu)
        resultClass = mantissa ? 0x11u : (negative ? 0x09u : 0x05u);
    else if (exponent == 0)
        resultClass = mantissa ? (negative ? 0x18u : 0x14u)
                               : (negative ? 0x12u : 0x02u);
    else
        resultClass = negative ? 0x08u : 0x04u;
    return resultClass << 12;
}

inline uint64_t PPC_FRSQRTE_VALUE(uint64_t bits, bool nonIEEE = false) noexcept
{
    constexpr uint64_t SignMask = 0x8000000000000000ull;
    constexpr uint64_t ExponentMask = 0x7FF0000000000000ull;
    constexpr uint64_t MantissaMask = 0x000FFFFFFFFFFFFFull;
    constexpr uint64_t QuietMask = 0x0008000000000000ull;
    constexpr uint64_t CanonicalQNaN = 0x7FF8000000000000ull;
    constexpr uint8_t EstimateTable[16] = {
        241, 216, 192, 168, 152, 136, 128, 112,
        96, 76, 60, 48, 32, 24, 16, 8
    };

    const bool negative = (bits & SignMask) != 0;
    uint32_t exponent = static_cast<uint32_t>((bits >> 52) & 0x7FFu);
    uint64_t mantissa = bits & MantissaMask;

    if (exponent == 0x7FFu && mantissa != 0)
        return bits | QuietMask;
    if (exponent == 0 && mantissa == 0)
        return (bits & SignMask) | ExponentMask;
    if (exponent == 0x7FFu && !negative)
        return 0;
    if (nonIEEE && exponent == 0)
        return (bits & SignMask) | ExponentMask;
    if (negative)
        return CanonicalQNaN;

    int32_t effectiveExponent = static_cast<int32_t>(exponent);
    uint64_t normalizedMantissa = mantissa;
    if (exponent == 0)
    {
        int leadingZeroes = 0;
        uint64_t scan = mantissa;
        while ((scan & SignMask) == 0)
        {
            scan <<= 1;
            ++leadingZeroes;
        }
        normalizedMantissa = mantissa << (leadingZeroes - 11);
        effectiveExponent = 12 - leadingZeroes;
    }

    const uint32_t topThree = static_cast<uint32_t>((normalizedMantissa >> 49) & 7u);
    const uint32_t index = ((((static_cast<uint32_t>(effectiveExponent) & 1u) << 3) |
                              topThree) ^ 8u);
    const int32_t unbiased = effectiveExponent - 1023;
    // C++ integer division truncates toward zero, while the architectural
    // exponent expression requires floor division for negative odd values.
    const int32_t half = unbiased >= 0 ? unbiased / 2 : -((-unbiased + 1) / 2);
    const uint32_t resultExponent = static_cast<uint32_t>(1022 - half);
    return (static_cast<uint64_t>(resultExponent) << 52) |
           (static_cast<uint64_t>(EstimateTable[index]) << 44);
}

inline void PPC_FRSQRTE(PPCRegister& destination, PPCFPSCRRegister& fpscr,
                        const PPCRegister& source, PPCCRRegister* cr1 = nullptr) noexcept
{
    constexpr uint64_t SignMask = 0x8000000000000000ull;
    constexpr uint64_t ExponentMask = 0x7FF0000000000000ull;
    constexpr uint64_t MantissaMask = 0x000FFFFFFFFFFFFFull;
    constexpr uint64_t QuietMask = 0x0008000000000000ull;

    const uint64_t bits = source.u64;
    const bool negative = (bits & SignMask) != 0;
    const uint32_t exponent = static_cast<uint32_t>((bits >> 52) & 0x7FFu);
    const uint64_t mantissa = bits & MantissaMask;
    const bool signalingNaN = exponent == 0x7FFu && mantissa != 0 &&
                              (mantissa & QuietMask) == 0;
    const bool invalidSqrt = negative && !signalingNaN &&
                             !(exponent == 0 && mantissa == 0) &&
                             !(exponent == 0x7FFu && mantissa != 0);
    const bool treatedAsZero = exponent == 0 &&
                               (mantissa == 0 || (fpscr.value & PPC_FPSCR_NI));

    bool resultEnabled = true;
    if (signalingNaN || invalidSqrt)
    {
        fpscr.value &= ~(PPC_FPSCR_FR | PPC_FPSCR_FI);
        fpscr.value |= PPC_FPSCR_FX | PPC_FPSCR_VX |
                       (signalingNaN ? PPC_FPSCR_VXSNAN : PPC_FPSCR_VXSQRT);
        if (fpscr.value & PPC_FPSCR_VE)
        {
            fpscr.value |= PPC_FPSCR_FEX;
            resultEnabled = false;
        }
    }
    else if (treatedAsZero)
    {
        fpscr.value &= ~(PPC_FPSCR_FR | PPC_FPSCR_FI);
        fpscr.value |= PPC_FPSCR_FX | PPC_FPSCR_ZX;
        if (fpscr.value & PPC_FPSCR_ZE)
        {
            fpscr.value |= PPC_FPSCR_FEX;
            resultEnabled = false;
        }
    }

    if (resultEnabled)
    {
        destination.u64 = PPC_FRSQRTE_VALUE(bits, (fpscr.value & PPC_FPSCR_NI) != 0);
        fpscr.value = (fpscr.value & ~PPC_FPSCR_FPRF) |
                      PPC_FPRF_FROM_F64(destination.u64);
    }

    if (cr1)
    {
        cr1->lt = (fpscr.value & PPC_FPSCR_FX) != 0;
        cr1->gt = (fpscr.value & PPC_FPSCR_FEX) != 0;
        cr1->eq = (fpscr.value & PPC_FPSCR_VX) != 0;
        cr1->so = (fpscr.value & PPC_FPSCR_OX) != 0;
    }
}

struct alignas(0x40) PPCContext
{
    PPCRegister r3;
#ifndef PPC_CONFIG_NON_ARGUMENT_AS_LOCAL
    PPCRegister r0;
#endif
    PPCRegister r1;
#ifndef PPC_CONFIG_NON_ARGUMENT_AS_LOCAL
    PPCRegister r2;
#endif
    PPCRegister r4;
    PPCRegister r5;
    PPCRegister r6;
    PPCRegister r7;
    PPCRegister r8;
    PPCRegister r9;
    PPCRegister r10;
#ifndef PPC_CONFIG_NON_ARGUMENT_AS_LOCAL
    PPCRegister r11;
    PPCRegister r12;
#endif
    PPCRegister r13;
#ifndef PPC_CONFIG_NON_VOLATILE_AS_LOCAL
    PPCRegister r14;
    PPCRegister r15;
    PPCRegister r16;
    PPCRegister r17;
    PPCRegister r18;
    PPCRegister r19;
    PPCRegister r20;
    PPCRegister r21;
    PPCRegister r22;
    PPCRegister r23;
    PPCRegister r24;
    PPCRegister r25;
    PPCRegister r26;
    PPCRegister r27;
    PPCRegister r28;
    PPCRegister r29;
    PPCRegister r30;
    PPCRegister r31;
#endif

#ifndef PPC_CONFIG_SKIP_LR
    uint64_t lr;
#endif
#ifndef PPC_CONFIG_CTR_AS_LOCAL
    PPCRegister ctr;
#endif
#ifndef PPC_CONFIG_XER_AS_LOCAL
    PPCXERRegister xer;
#endif
#ifndef PPC_CONFIG_RESERVED_AS_LOCAL
    PPCRegister reserved;
#endif
#ifndef PPC_CONFIG_SKIP_MSR
    uint32_t msr = 0x200A000;
#endif
#ifndef PPC_CONFIG_CR_AS_LOCAL
    PPCCRRegister cr0;
    PPCCRRegister cr1;
    PPCCRRegister cr2;
    PPCCRRegister cr3;
    PPCCRRegister cr4;
    PPCCRRegister cr5;
    PPCCRRegister cr6;
    PPCCRRegister cr7;
#endif
    PPCFPSCRRegister fpscr;
    uint32_t vscr = 0;

#ifndef PPC_CONFIG_NON_ARGUMENT_AS_LOCAL
    PPCRegister f0;
#endif
    PPCRegister f1;
    PPCRegister f2;
    PPCRegister f3;
    PPCRegister f4;
    PPCRegister f5;
    PPCRegister f6;
    PPCRegister f7;
    PPCRegister f8;
    PPCRegister f9;
    PPCRegister f10;
    PPCRegister f11;
    PPCRegister f12;
    PPCRegister f13;
#ifndef PPC_CONFIG_NON_VOLATILE_AS_LOCAL
    PPCRegister f14;
    PPCRegister f15;
    PPCRegister f16;
    PPCRegister f17;
    PPCRegister f18;
    PPCRegister f19;
    PPCRegister f20;
    PPCRegister f21;
    PPCRegister f22;
    PPCRegister f23;
    PPCRegister f24;
    PPCRegister f25;
    PPCRegister f26;
    PPCRegister f27;
    PPCRegister f28;
    PPCRegister f29;
    PPCRegister f30;
    PPCRegister f31;
#endif

    PPCVRegister v0;
    PPCVRegister v1;
    PPCVRegister v2;
    PPCVRegister v3;
    PPCVRegister v4;
    PPCVRegister v5;
    PPCVRegister v6;
    PPCVRegister v7;
    PPCVRegister v8;
    PPCVRegister v9;
    PPCVRegister v10;
    PPCVRegister v11;
    PPCVRegister v12;
    PPCVRegister v13;
#ifndef PPC_CONFIG_NON_VOLATILE_AS_LOCAL
    PPCVRegister v14;
    PPCVRegister v15;
    PPCVRegister v16;
    PPCVRegister v17;
    PPCVRegister v18;
    PPCVRegister v19;
    PPCVRegister v20;
    PPCVRegister v21;
    PPCVRegister v22;
    PPCVRegister v23;
    PPCVRegister v24;
    PPCVRegister v25;
    PPCVRegister v26;
    PPCVRegister v27;
    PPCVRegister v28;
    PPCVRegister v29;
    PPCVRegister v30;
    PPCVRegister v31;
#endif
#ifndef PPC_CONFIG_NON_ARGUMENT_AS_LOCAL
    PPCVRegister v32;
    PPCVRegister v33;
    PPCVRegister v34;
    PPCVRegister v35;
    PPCVRegister v36;
    PPCVRegister v37;
    PPCVRegister v38;
    PPCVRegister v39;
    PPCVRegister v40;
    PPCVRegister v41;
    PPCVRegister v42;
    PPCVRegister v43;
    PPCVRegister v44;
    PPCVRegister v45;
    PPCVRegister v46;
    PPCVRegister v47;
    PPCVRegister v48;
    PPCVRegister v49;
    PPCVRegister v50;
    PPCVRegister v51;
    PPCVRegister v52;
    PPCVRegister v53;
    PPCVRegister v54;
    PPCVRegister v55;
    PPCVRegister v56;
    PPCVRegister v57;
    PPCVRegister v58;
    PPCVRegister v59;
    PPCVRegister v60;
    PPCVRegister v61;
    PPCVRegister v62;
    PPCVRegister v63;
#endif
#ifndef PPC_CONFIG_NON_VOLATILE_AS_LOCAL
    PPCVRegister v64;
    PPCVRegister v65;
    PPCVRegister v66;
    PPCVRegister v67;
    PPCVRegister v68;
    PPCVRegister v69;
    PPCVRegister v70;
    PPCVRegister v71;
    PPCVRegister v72;
    PPCVRegister v73;
    PPCVRegister v74;
    PPCVRegister v75;
    PPCVRegister v76;
    PPCVRegister v77;
    PPCVRegister v78;
    PPCVRegister v79;
    PPCVRegister v80;
    PPCVRegister v81;
    PPCVRegister v82;
    PPCVRegister v83;
    PPCVRegister v84;
    PPCVRegister v85;
    PPCVRegister v86;
    PPCVRegister v87;
    PPCVRegister v88;
    PPCVRegister v89;
    PPCVRegister v90;
    PPCVRegister v91;
    PPCVRegister v92;
    PPCVRegister v93;
    PPCVRegister v94;
    PPCVRegister v95;
    PPCVRegister v96;
    PPCVRegister v97;
    PPCVRegister v98;
    PPCVRegister v99;
    PPCVRegister v100;
    PPCVRegister v101;
    PPCVRegister v102;
    PPCVRegister v103;
    PPCVRegister v104;
    PPCVRegister v105;
    PPCVRegister v106;
    PPCVRegister v107;
    PPCVRegister v108;
    PPCVRegister v109;
    PPCVRegister v110;
    PPCVRegister v111;
    PPCVRegister v112;
    PPCVRegister v113;
    PPCVRegister v114;
    PPCVRegister v115;
    PPCVRegister v116;
    PPCVRegister v117;
    PPCVRegister v118;
    PPCVRegister v119;
    PPCVRegister v120;
    PPCVRegister v121;
    PPCVRegister v122;
    PPCVRegister v123;
    PPCVRegister v124;
    PPCVRegister v125;
    PPCVRegister v126;
    PPCVRegister v127;
#endif
};

inline uint8_t VectorMaskL[] =
{
    0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
    0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
    0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
    0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
    0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D, 0x0C,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E, 0x0D,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0E,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,
};

inline uint8_t VectorMaskR[] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF, 0xFF,
    0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF, 0xFF,
    0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFF,
};

inline uint8_t VectorShiftTableL[] =
{
    0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
    0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
    0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
    0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
    0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
    0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05,
    0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
    0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07,
    0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
    0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09,
    0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A,
    0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B,
    0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C,
    0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D,
    0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E,
    0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F,
};

inline uint8_t VectorShiftTableR[] =
{
    0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
    0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F,
    0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E,
    0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D,
    0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C,
    0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B,
    0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A,
    0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09,
    0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
    0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07,
    0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06,
    0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05,
    0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
    0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
    0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02,
    0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
};

inline simde__m128i simde_mm_adds_epu32(simde__m128i a, simde__m128i b)
{
    return simde_mm_add_epi32(a, simde_mm_min_epu32(simde_mm_xor_si128(a, simde_mm_cmpeq_epi32(a, a)), b));
}

inline simde__m128i simde_mm_avg_epi8(simde__m128i a, simde__m128i b)
{
    simde__m128i c = simde_mm_set1_epi8(char(128));
    return simde_mm_xor_si128(c, simde_mm_avg_epu8(simde_mm_xor_si128(c, a), simde_mm_xor_si128(c, b)));
}

inline simde__m128i simde_mm_avg_epi16(simde__m128i a, simde__m128i b)
{
    simde__m128i c = simde_mm_set1_epi16(short(32768));
    return simde_mm_xor_si128(c, simde_mm_avg_epu16(simde_mm_xor_si128(c, a), simde_mm_xor_si128(c, b)));
}

inline simde__m128 simde_mm_cvtepu32_ps_(simde__m128i src1)
{
    simde__m128i xmm1 = simde_mm_add_epi32(src1, simde_mm_set1_epi32(127));
    simde__m128i xmm0 = simde_mm_slli_epi32(src1, 31 - 8);
    xmm0 = simde_mm_srli_epi32(xmm0, 31);
    xmm0 = simde_mm_add_epi32(xmm0, xmm1);
    xmm0 = simde_mm_srai_epi32(xmm0, 8);
    xmm0 = simde_mm_add_epi32(xmm0, simde_mm_set1_epi32(0x4F800000));
    simde__m128 xmm2 = simde_mm_cvtepi32_ps(src1);
    return simde_mm_blendv_ps(xmm2, simde_mm_castsi128_ps(xmm0), simde_mm_castsi128_ps(src1));
}

inline simde__m128i simde_mm_perm_epi8_(simde__m128i a, simde__m128i b, simde__m128i c)
{
    simde__m128i d = simde_mm_set1_epi8(0xF);
    simde__m128i e = simde_mm_sub_epi8(d, simde_mm_and_si128(c, d));
    return simde_mm_blendv_epi8(simde_mm_shuffle_epi8(a, e), simde_mm_shuffle_epi8(b, e), simde_mm_slli_epi32(c, 3));
}

inline simde__m128i simde_mm_cmpgt_epu8(simde__m128i a, simde__m128i b)
{
    simde__m128i c = simde_mm_set1_epi8(char(128));
    return simde_mm_cmpgt_epi8(simde_mm_xor_si128(a, c), simde_mm_xor_si128(b, c));
}

inline simde__m128i simde_mm_cmpgt_epu16(simde__m128i a, simde__m128i b)
{
    simde__m128i c = simde_mm_set1_epi16(short(32768));
    return simde_mm_cmpgt_epi16(simde_mm_xor_si128(a, c), simde_mm_xor_si128(b, c));
}

inline simde__m128i simde_mm_vctsxs(simde__m128 src1)
{
    simde__m128 xmm2 = simde_mm_cmpunord_ps(src1, src1);
    simde__m128i xmm0 = simde_mm_cvttps_epi32(src1);
    simde__m128i xmm1 = simde_mm_cmpeq_epi32(xmm0, simde_mm_set1_epi32(INT_MIN));
    xmm1 = simde_mm_andnot_si128(simde_mm_castps_si128(src1), xmm1);
    simde__m128 dest = simde_mm_blendv_ps(simde_mm_castsi128_ps(xmm0), simde_mm_castsi128_ps(simde_mm_set1_epi32(INT_MAX)), simde_mm_castsi128_ps(xmm1));
    return simde_mm_andnot_si128(simde_mm_castps_si128(xmm2), simde_mm_castps_si128(dest));
}

// vctuxs / vcfpuxws128: float to unsigned word, truncating and saturating
// (NaN and negative values give 0, values from 2^32 give 0xFFFFFFFF).
inline simde__m128i simde_mm_vctuxs(simde__m128 src1)
{
    // maxps returns its second operand for NaN, so NaN becomes 0.
    simde__m128 clamped = simde_mm_max_ps(src1, simde_mm_setzero_ps());
    const simde__m128 two31 = simde_mm_set1_ps(2147483648.0f);
    simde__m128 high = simde_mm_cmpge_ps(clamped, two31);
    simde__m128i result = simde_mm_cvttps_epi32(simde_mm_sub_ps(clamped, simde_mm_and_ps(high, two31)));
    result = simde_mm_add_epi32(result, simde_mm_and_si128(simde_mm_castps_si128(high), simde_mm_set1_epi32(INT_MIN)));
    simde__m128 over = simde_mm_cmpge_ps(clamped, simde_mm_set1_ps(4294967296.0f));
    return simde_mm_or_si128(result, simde_mm_castps_si128(over));
}

inline simde__m128i simde_mm_vsr(simde__m128i a, simde__m128i b)
{
    b = simde_mm_srli_epi64(simde_mm_slli_epi64(b, 61), 61);
    return simde_mm_castps_si128(simde_mm_insert_ps(simde_mm_castsi128_ps(simde_mm_srl_epi64(a, b)), simde_mm_castsi128_ps(simde_mm_srl_epi64(simde_mm_srli_si128(a, 4), b)), 0x10));
}

#if defined(__aarch64__) || defined(_M_ARM64)
inline uint64_t __rdtsc()
{
    uint64_t ret;
    asm volatile("mrs %0, cntvct_el0\n\t"
                 : "=r"(ret)::"memory");
    return ret;
}
#elif !defined(__x86_64__) && !defined(_M_X64)
#   error "Missing implementation for __rdtsc()"
#endif

#endif
