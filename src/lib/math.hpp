template <typename T>
T div_roundup(T a, T b) {
    return (a + (b - 1)) / b;
}
