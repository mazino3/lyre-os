#pragma once

#define DIV_ROUNDUP(A, B) ({ \
    typeof(A) _a_ = A;       \
    typeof(B) _b_ = B;       \
    (_a_ + (_b_ - 1)) / _b_; \
})

#define ALIGN_UP(A, B) ({        \
    typeof(A) _a__ = A;           \
    typeof(B) _b__ = B;           \
    DIV_ROUNDUP(_a__, _b__) * _b__; \
})

#define ALIGN_DOWN(A, B) ({ \
    typeof(A) _a_ = A;      \
    typeof(B) _b_ = B;      \
    (_a_ / _b_) * _b_;      \
})
