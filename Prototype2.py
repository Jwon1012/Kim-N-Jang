import numpy as np
import matplotlib.pyplot as plt
import sympy as sp

# ==========================================
# 1. 수식 파서(Parser): 수식 입력 및 해석
# ==========================================

x = sp.symbols('x')

# 사용자가 입력한 함수
f_expr = sp.sympify("x**2")
g_expr = sp.sympify("2*x")

# 컴퓨터가 계산할 수 있는 함수로 변환
f = sp.lambdify(x, f_expr, "numpy")
g = sp.lambdify(x, g_expr, "numpy")

print("함수 f(x):", f_expr)
print("함수 g(x):", g_expr)


# ==========================================
# 2. 샘플링: 좌표 생성 및 그래프 표현
# ==========================================

x_points = np.linspace(-2, 4, 100)

y_points = f(x_points)

# 샘플링된 좌표 출력
print("\n[샘플링 좌표]")
for i in range(0, len(x_points), 20):
    print(
        f"x={x_points[i]:.2f}, "
        f"y={y_points[i]:.2f}"
    )


# ==========================================
# 3. 미분: 중앙 차분법
# ==========================================

def numerical_derivative(func, x_value, h=0.0001):
    return (
        func(x_value + h) - func(x_value - h)
    ) / (2 * h)


target_x = 2

slope = numerical_derivative(f, target_x)

print("\n[미분 결과]")
print("x =", target_x)
print("접선의 기울기 =", slope)


# ==========================================
# 4. 적분: 사다리꼴 공식
# ==========================================

def numerical_integral(func, a, b, n=1000):
    xs = np.linspace(a, b, n + 1)
    ys = func(xs)

    h = (b - a) / n

    area = h * (
        np.sum(ys) - (ys[0] + ys[-1]) / 2
    )

    return area


area = numerical_integral(f, 0, 2)

print("\n[적분 결과]")
print("적분 구간: 0 ~ 2")
print("넓이의 근삿값 =", area)


# ==========================================
# 5. 교점 계산: 이분법
# ==========================================

def bisection(func, a, b, tolerance=1e-8):
    if func(a) * func(b) > 0:
        raise ValueError("구간 양 끝의 함수값 부호가 달라야 합니다.")

    while abs(b - a) > tolerance:
        mid = (a + b) / 2

        if abs(func(mid)) < tolerance:
            return mid

        if func(a) * func(mid) < 0:
            b = mid
        else:
            a = mid

    return (a + b) / 2


# 두 함수의 차이가 0이 되는 지점 탐색
def difference(t):
    return f(t) - g(t)


root1 = bisection(difference, -1, 1)
root2 = bisection(difference, 1, 3)

print("\n[교점 계산 결과]")
print("첫 번째 교점:", root1, f(root1))
print("두 번째 교점:", root2, f(root2))


# ==========================================
# 6. 그래프 시각화
# ==========================================

xs = np.linspace(-2, 4, 500)

plt.figure(figsize=(10, 7))

# 두 함수 그래프
plt.plot(xs, f(xs), label="f(x) = x^2")
plt.plot(xs, g(xs), label="g(x) = 2x")

# 샘플링된 점 연결
sample_x = np.linspace(-2, 4, 13)
plt.plot(
    sample_x,
    f(sample_x),
    "o--",
    label="Sampled points"
)

# 적분 영역 표시
area_x = np.linspace(0, 2, 100)
plt.fill_between(
    area_x,
    f(area_x),
    0,
    alpha=0.2,
    label="Integral area"
)

# 접선 표시
tangent_y = slope * (xs - target_x) + f(target_x)
plt.plot(xs, tangent_y, "--", label="Tangent at x=2")

# 교점 표시
plt.scatter(
    [root1, root2],
    [f(root1), f(root2)],
    color="red",
    s=70,
    label="Intersections",
    zorder=5
)

plt.axhline(0, color="black", linewidth=0.7)
plt.axvline(0, color="black", linewidth=0.7)

plt.xlabel("x")
plt.ylabel("y")
plt.title("Calculus Simulation using Python")

plt.grid(True)
plt.legend()
plt.show()
