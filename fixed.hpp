#pragma once

#include <cstdint>
#include <cmath>

const float epsilon = 0.000001;

class Fixed {
public:
    Fixed(float in, bool allowApproximate = false) {
        float input = in * 65536.0f;
        float rounded = std::round(input);
        float diff = fabsf(rounded - input);
        if (diff > epsilon && !allowApproximate) {
            abort();
        }

        data = (int64_t)rounded;
    }

    Fixed(double in, bool allowApproximate = false) {
        double input = in * 65536.0f;
        double rounded = std::round(input);
        double diff = fabs(rounded - input);
        if (diff > epsilon && !allowApproximate) {
            abort();
        }

        data = (int64_t)rounded;
    }

    Fixed(int in) {
        data = in << 16;
    }

    Fixed(int64_t in) {
        data = in << 16;
    }

    Fixed() {
        data = 0;
    }

    bool operator == (const Fixed& rhs) {
        if (data == rhs.data) {
            return true;
        }

        return false;
    }

    bool operator != (const Fixed& rhs) {
        if (data != rhs.data) {
            return true;
        }

        return false;
    }

    bool operator < (const Fixed& rhs) {
        if (data < rhs.data) {
            return true;
        }

        return false;
    }

    bool operator <= (const Fixed& rhs) {
        if (data <= rhs.data) {
            return true;
        }

        return false;
    }

    bool operator > (const Fixed& rhs) {
        if (data > rhs.data) {
            return true;
        }

        return false;
    }

    bool operator >= (const Fixed& rhs) {
        if (data >= rhs.data) {
            return true;
        }

        return false;
    }

    Fixed operator + (const Fixed &in) {
        Fixed ret;
        ret.data = data + in.data;
        return ret;
    }

    Fixed operator += (const Fixed &in) {
        Fixed ret;
        ret.data = data + in.data;
        data = ret.data;
        return ret;
    }

    Fixed operator - (const Fixed &in) {
        Fixed ret;
        ret.data = data - in.data;
        return ret;
    }

    Fixed operator -= (const Fixed &in) {
        Fixed ret;
        ret.data = data - in.data;
        data = ret.data;
        return ret;
    }

    Fixed operator / (const Fixed &in) {
        Fixed ret;
        ret.data = (data << 16) / in.data;
        return ret;
    }

    Fixed operator /= (const Fixed &in) {
        Fixed ret;
        ret.data = (data << 16) / in.data;
        data = ret.data;
        return ret;
    }

    Fixed operator * (const Fixed &in) const {
        Fixed ret;
        ret.data = (data * in.data) >> 16;
        return ret;
    }

    Fixed operator *= (const Fixed &in) {
        Fixed ret;
        ret.data = (data * in.data) >> 16;
        data = ret.data;
        return ret;
    }

    Fixed operator - () const {
        Fixed ret;
        ret = *this * Fixed(-1);
        return ret;
    }

    float f() { return data / 65536.0f; }
    int i() { return data >> 16; }

    friend Fixed fixAbs( Fixed fix ) {
        if (fix.data < 0) {
            return -fix;
        }

        return fix;
    }

    friend Fixed fixMax( Fixed a, Fixed b ) {
        if (b > a) {
            return b;
        }

        return a;
    }

    friend Fixed fixMin( Fixed a, Fixed b ) {
        if (b < a) {
            return b;
        }

        return a;
    }

    friend Fixed fixDivWithBias( Fixed a, Fixed b) {
        Fixed ret = a / b;
        // int remainder = a.data - b.data * ret.data;
        // remainder /= std::abs(remainder);
        if (std::abs(ret.data) & 63) {
            ret.data += ret.i() / std::abs(ret.i());
        }
        return ret;
    }

    int64_t data = 0;
};