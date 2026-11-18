import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from scipy.integrate import quad
from scipy.optimize import fsolve
# ==============================

# 한글 폰트 설정

# ==============================
# Windows
plt.rcParams['font.family'] = 'Malgun Gothic'
# Mac 사용 시 아래로 변경
# plt.rcParams['font.family'] = 'AppleGothic'
# Linux 사용 시 아래로 변경
# plt.rcParams['font.family'] = 'NanumGothic'
plt.rcParams['axes.unicode_minus'] = False
# ==============================
# 사용자 함수 입력
# ==============================

expr = input("함수를 입력하세요 (예: sin(x), x**2, exp(x), x**3-3*x+1) : ")

# 함수 정의
def f(x):
    return eval(
        expr,
        {
            "__builtins__": None,
            "np": np,
            "sin": np.sin,
            "cos": np.cos,
            "tan": np.tan,
            "exp": np.exp,
            "log": np.log,
            "sqrt": np.sqrt,
            "abs": np.abs,
            "pi": np.pi,
            "e": np.e,
            "x": x
        }
    )

# ==============================
# 그래프 범위 입력
# ==============================

xmin = float(input("x 시작값 : "))
xmax = float(input("x 끝값 : "))

# ==============================
# 함수 데이터 생성
# ==============================

x = np.linspace(xmin, xmax, 2000)

try:
    y = f(x)
except Exception as e:
    print("함수 계산 오류:", e)
    exit()

# ==============================
# 함수 그래프 표시
# ==============================

fig, ax = plt.subplots(figsize=(10, 6))

ax.plot(
    x,
    y,
    label=f"입력 함수 : {expr}",
    linewidth=2
)

ax.set_title("그래프에서 두 점을 선택하세요")
ax.set_xlabel("x축")
ax.set_ylabel("y축")
ax.grid(True)
ax.legend()

print("\n그래프에서 두 점을 클릭하세요.")

selected = plt.ginput(2, timeout=-1)

plt.close()

(x1, y1), (x2, y2) = selected

print("\n선택한 점")
print(f"점 1 : ({x1:.6f}, {y1:.6f})")
print(f"점 2 : ({x2:.6f}, {y2:.6f})")

# ==============================
# 직선 생성
# ==============================

if abs(x2 - x1) < 1e-10:
    print("수직선은 지원하지 않습니다.")
    exit()

m = (y2 - y1) / (x2 - x1)
c = y1 - m * x1

def line(x):
    return m * x + c

print("\n선택한 두 점을 지나는 직선")
print(f"y = {m:.6f}x + {c:.6f}")

# ==============================
# 수치 미분
# ==============================

def derivative(x0, h=1e-6):
    return (f(x0 + h) - f(x0 - h)) / (2 * h)

d1 = derivative(x1)
d2 = derivative(x2)

print("\n미분값")

print(f"f'({x1:.6f}) = {d1:.6f}")
print(f"f'({x2:.6f}) = {d2:.6f}")

# ==============================
# 정적분
# ==============================

a = min(x1, x2)
b = max(x1, x2)

integral_value, error = quad(f, a, b)

print("\n정적분 결과")
print(f"∫[{a:.6f}, {b:.6f}] f(x)dx")
print(f"= {integral_value:.6f}")

# ==============================
# 교점 계산
# ==============================

def difference(x):
    return f(x) - line(x)

roots = []

search_x = np.linspace(xmin, xmax, 500)

for i in range(len(search_x) - 1):

    xa = search_x[i]
    xb = search_x[i + 1]

    try:

        if difference(xa) * difference(xb) < 0:

            root = fsolve(
                difference,
                (xa + xb) / 2
            )[0]

            if xmin <= root <= xmax:

                duplicate = False

                for r in roots:
                    if abs(r - root) < 1e-3:
                        duplicate = True

                if not duplicate:
                    roots.append(root)

    except:
        pass

# ==============================
# 교점 출력
# ==============================

print("\n교점 정보")

if len(roots) == 0:
    print("교점이 없습니다.")

for i, r in enumerate(roots):

    print(
        f"{i+1}번 교점 : "
        f"({r:.6f}, {f(r):.6f})"
    )

# ==============================
# 결과 시각화
# ==============================

plt.figure(figsize=(12, 7))

plt.plot(
    x,
    f(x),
    color="blue",
    linewidth=2,
    label="함수 그래프"
)

plt.plot(
    x,
    line(x),
    '--',
    color="red",
    linewidth=2,
    label="선택된 직선"
)

plt.scatter(
    [x1, x2],
    [y1, y2],
    color="black",
    s=120,
    label="선택한 점"
)

# 교점 표시

if len(roots) > 0:

    plt.scatter(
        roots,
        [f(r) for r in roots],
        color="green",
        s=150,
        label="교점"
    )

# 적분 영역 표시

mask = (x >= a) & (x <= b)

plt.fill_between(
    x,
    f(x),
    0,
    where=mask,
    color="orange",
    alpha=0.3,
    label="적분 영역"
)

plt.title("함수 분석 결과", fontsize=15)

plt.xlabel("x축", fontsize=12)
plt.ylabel("y축", fontsize=12)

plt.grid(True)
plt.legend()
plt.show()