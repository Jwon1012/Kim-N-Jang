import sys
import os
import ctypes
import numpy as np
from PyQt6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton
import pyqtgraph as pg

# 1. C++ DLL 로드 (상대 경로)
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DLL_PATH = os.path.join(BASE_DIR, "CalcTech_C_Project", "x64", "Debug", "CalcTechCProject.dll")

try:
    c_engine = ctypes.CDLL(DLL_PATH)
    # generate_sin_data(double start, double end, int n_points, double* x_out, double* y_out)
    c_engine.generate_sin_data.argtypes = [
        ctypes.c_double, ctypes.c_double, ctypes.c_int,
        ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)
    ] #
    c_engine.generate_sin_data.restype = None
    print("✅ C++ DLL 로드 성공!")
except Exception as e:
    print(f"❌ DLL 로드 실패: {e}")
    sys.exit(1)

# 2. PyQt6 GUI 메인 창 클래스
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Calculus Simulator - Sin(x) Graph")
        self.resize(800, 600)

        # 메인 레이아웃 세팅
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # PyQtGraph 위젯 생성
        self.graph_widget = pg.PlotWidget()
        self.graph_widget.setBackground('w') # 흰색 배경
        self.graph_widget.showGrid(x=True, y=True)
        self.graph_widget.setLabel('left', 'y = sin(x)')
        self.graph_widget.setLabel('bottom', 'x')
        layout.addWidget(self.graph_widget)

        # 그래프 그리기 버튼
        self.btn_draw = QPushButton("C++ 연산 호출 및 그래프 출력")
        self.btn_draw.clicked.connect(self.plot_sin_wave)
        layout.addWidget(self.btn_draw)

    def plot_sin_wave(self):
        n_points = 500 #배열의 방 개수 (혹은 크기)
        start, end = 0.0, 4.0 * np.pi #함수의 정의역 설정
        #잘만 스면 진짜 지오지브라처럼 그래프의 범위를 실시간으로 조작할 수 있을듯

        # C++ DLL 함수로 전달할 배열 준비
        double_array_type = ctypes.c_double * n_points #
        x_arr = double_array_type() #500칸 짜리 빈 배열상자 x,y 생성
        y_arr = double_array_type()

        # C++ 커널 연산 수행 (y_arr상자에 sin(x)결과 데이터를 가득 채워넣기)
        c_engine.generate_sin_data(start, end, n_points, x_arr, y_arr)

        # 파이썬 리스트로 변환 후 그래프 렌더링
        x_data = np.frombuffer(x_arr, dtype = np.float64)
        y_data = np.frombuffer(y_arr, dtype = np.float64)

        self.graph_widget.clear() #화면 렌더링
        self.graph_widget.plot(x_data, y_data, pen=pg.mkPen(color='b', width=2))

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())