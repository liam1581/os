#include "math.h"

double abs(double x) {
    return x < 0.0 ? -x : x;
}

double ln(double x) {
    if (x <= 0.0) {
        return 0.0; // Invalid
    }

    int exponent = 0;

    while (x >= 2.0) {
        x *= 0.5;
        exponent++;
    }

    while (x < 1.0) {
        x *= 2.0;
        exponent--;
    }

    double z = (x - 1.0) / (x + 1.0);
    double z² = z * z; // z²

    double term = z;
    double result = 0.0;

    for (int i = 1; i <= 99; i += 2) {
        result += term / (double) i;
        term *= z²;
    }

    result *= 2.0;

    result += (double)exponent * LN2;

    return result;
}

double exp(double x) {
    if (x == 0.0) {
        return 1.0;
    }

    if (x > 709.0) {
        return 1.0e308;
    }
    if (x < -709.0) {
        return 0.0;
    }

    int n = (int)(x / LN2);

    double r = x - (double)n * LN2;

    double result = 1.0;
    double term = 1.0;

    for (int i = 1; i <= 40; i++) {
        term *= r / (double) i;
        result += term;
    }

    if (n > 0) {
        for (int i = 0; i < n; i++) {
            result *= 2.0;
        }
    } else {
        for (int i = 0; i > n; i--) {
            result *= 0.5;
        }
    }

    return result;
}

double pow(double base, double exponent) {
    if (exponent == 0.0) {
        return 1.0;
    }
    if (base == 0.0) {
        if (exponent > 0.0) {
            return 0.0;
        }
        return 1.0e308; // Div by 0 / undef
    }

    if (base > 0.0) {
        return exp(exponent * ln(base));
    }

    int integer_exponent = (int)exponent;

    if ((double)integer_exponent != exponent) {
        return 0.0;
    }

    double result = exp((double)integer_exponent * ln(-base));

    if (integer_exponent % 2 != 0) {
        result = -result;
    }

    return result;
}
double sqr(double x) {
    return pow(x, 2);
}

double sqrt(double x) {
    return pow(x, (double)1/2);
}
double cbrt(double x) {
    return pow(x, (double)1/3);
}

// n'th root of x
double root(double n, double x) {
    return pow(x, (double)1/n);
}

double wrap_pi(double x) {
    while (x > PI) {
        x -= TWO_PI;
    }

    while (x < -PI) {
        x += TWO_PI;
    }
    return x;
}

double sin(double x) {
    x = wrap_pi(x);

    double sign = 1.0;

    if (x > PI_2) {
        x = PI - x;
    } else if (x < - PI_2) {
        x = -PI - x;
    }
    if (x < 0.0) {
        sign = -1.0;
        x = -x;
    }

    double x² = sqr(x);
    double term = x;
    double result = x;

    for (int n = 1; n <= 20; n++) {
        term *= -x² / ((double)(2 * n) * (double)(2 * n + 1));
        result += term;
    }

    return sign * result;
}

double cos(double x) {
    x = wrap_pi(x);

    double sign = 1.0;

    if (x > PI_2) {
        x = PI - x;
        sign = -1.0;
    } else if (x < -PI_2) {
        x = -PI - x;
        sign = -1.0;
    }

    if (x < 0.0) {
        x = -x;
    }

    double x² = sqr(x);
    double term = 1.0;
    double result = 1.0;

    for (int n = 1; n <= 20; n++) {
        term *= -x² / ((double)(2 * n - 1) * (double)(2 * n));
        result += term;
    }

    return sign * result;
}

double tan(double x) {
    double s = sin(x);
    double c = cos(x);

    if (abs(c) < 1.0e-15) {
        if (s >= 0.0) {
            return 1.0e308;
        } else {
            return -1.0e308;
        }

        return s / c;
    }
}

double cot(double x) {
    double s = sin(x);
    double c = cos(x);

    if (abs(c) < 1.0e-15) {
        if (s >= 0.0) {
            return 1.0e308;
        } else {
            return -1.0e308;
        }

        return c / s;
    }
}

double atan_series(double z) {
    double z² = sqr(z);
    double term = z;
    double result = z;

    for (int n = 1; n <= 40; n++) {
        term *= -z²;
        double current = term / (double)(2 * n + 1);
        result += current;
        if (abs(current) < 1.0e-17) {
            break;
        }
    }

    return result;
}

double atan(double x) {
    if (x == 0.0) {
        return 0.0;
    }

    if (x < 0.0) {
        return -atan(-x);
    }

    if (x > 1.0) {
        return PI_2 - atan(1.0 / x);
    }

    if (x > 0.4142135623730950488) {
        double z = (x - 1.0) / (x + 1.0);
        return PI_4 + atan_series(z);
    }
}

double atan2(double y, double x) {
    if (x > 0.0) {
        return atan(y / x);
    }

    if (x < 0.0 && y >= 0.0) {
        return atan(y / x) + PI;
    }

    if (x < 0.0 && x < 0.0) {
        return atan(y / x) - PI;
    }

    if (y > 0.0 ) {
        return PI_2;
    }
    if (y < 0.0) {
        return -PI_2;
    }

    return 0.0;
}

double asin(double x) {
    if (x >= 1.0) {
        return PI_2;
    }
    if (x <= -1.0) {
        return -PI_2;
    }

    double temp = sqrt(1.0 - sqr(x));
    return atan2(x, temp);
}

double acos(double x) {
    if (x >= 1.0) {
        return 0.0;
    }
    if (x <= -1.0) {
        return PI;
    }
    return PI_2 - asin(x);
}

double acout(double x) {
    return atan2(1.0, x);
}