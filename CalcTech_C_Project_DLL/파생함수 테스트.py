import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSlider, QLabel, QPushButton
from PyQt6.QtCore import Qt, QTimer

def f(x):
    return 0.5 * x**3 - 2 * x

def df_numerical(x, h=1e-5):
    return (f(x + h) - f(x - h)) / (2 * h)
class DraggablePIPDerivativeTrackerApp(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("CalcTech - Draggable PIP Derivative Tracker")
        self.setGeometry(100, 100, 950, 600)

        self.x_min, self.x_max = -3.0, 3.0
        self.current_a = None
        self.visited_a = []

        # --- PIP 드래그 상태 관리 변수 ---
        self.is_dragging_pip = False
        self.drag_start_pos = None
        self.pip_bbox_start = None

        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        main_layout.setContentsMargins(8, 8, 8, 8)

        plt.style.use('dark_background')
        self.fig = plt.figure(figsize=(9, 5.5), facecolor='#1e1e1e')
        
        # 1. 메인 평면
        self.ax1 = self.fig.add_axes([0.08, 0.15, 0.88, 0.78])
        self.ax1.set_facecolor('#252526')

        # 2. 서브 PIP 평면 (초기 위치: 우측 하단)
        self.pip_pos = [0.62, 0.20, 0.32, 0.32] # [left, bottom, width, height]
        self.ax2 = self.fig.add_axes(self.pip_pos)
        self.ax2.set_facecolor('#181818')
        
        self.canvas = FigureCanvas(self.fig)
        main_layout.addWidget(self.canvas)

        # --- 마우스 드래그 이벤트 연결 ---
        self.canvas.mpl_connect('button_press_event', self.on_mouse_press)
        self.canvas.mpl_connect('motion_notify_event', self.on_mouse_move)
        self.canvas.mpl_connect('button_release_event', self.on_mouse_release)

        # 하단 컨트롤러
        controls_layout = QHBoxLayout()
        self.play_btn = QPushButton("고속 탐색 ▶▶")
        self.play_btn.setFixedWidth(100)
        self.play_btn.clicked.connect(self.toggle_auto_play)
        controls_layout.addWidget(self.play_btn)

        self.label = QLabel("f(x) 평면 클릭 -> PIP 활성화 | PIP 창 드래그 -> 위치 이동")
        self.label.setStyleSheet("color: #aaaaaa; font-weight: bold;")
        controls_layout.addWidget(self.label)

        self.slider = QSlider(Qt.Orientation.Horizontal)
        self.slider.setRange(-300, 300)
        self.slider.setValue(0)
        self.slider.valueChanged.connect(self.on_slider_change)
        controls_layout.addWidget(self.slider)

        main_layout.addLayout(controls_layout)

        self.timer = QTimer()
        self.timer.setInterval(10)
        self.timer.timeout.connect(self.auto_step)
        self.sweep_direction = 1

        self.init_plots()

    def init_plots(self):
        self.x_vals = np.linspace(self.x_min, self.x_max, 500)
        
        # 메인 f(x)
        self.line_f, = self.ax1.plot(self.x_vals, f(self.x_vals), color='#ff9800', lw=2.5, label=r'$f(x) = 0.5x^3 - 2x$')
        self.line_tangent, = self.ax1.plot([], [], color='#ff5252', linestyle='--', lw=1.5, label='Tangent')
        self.point_tangent, = self.ax1.plot([], [], 'ro', markersize=8)
        self.ax1.set_ylabel('f(x)')
        self.ax1.legend(loc='upper left', fontsize=9)
        self.ax1.grid(True, alpha=0.2)
        self.ax1.set_title("Main Plane: f(x) [Click to Open PIP | Drag PIP to Move]", fontsize=11, pad=8)

        # 서브 PIP f'(x)
        self.ax2.set_visible(False)
        self.line_exact_df, = self.ax2.plot(self.x_vals, df_numerical(self.x_vals), color='gray', linestyle=':', alpha=0.4, label=r'$f\'(x)$')
        self.line_locus, = self.ax2.plot([], [], color='#00e676', lw=2, label='Locus')
        self.point_slope, = self.ax2.plot([], [], 'go', markersize=7, zorder=5)
        self.v_line = self.ax2.axvline(0, color='gray', linestyle=':', alpha=0.5)
        
        self.ax2.set_ylabel("f'(x)", fontsize=8)
        self.ax2.set_ylim(-3, 12)
        self.ax2.legend(loc='upper left', fontsize=7)
        self.ax2.grid(True, alpha=0.2)
        
        for spine in self.ax2.spines.values():
            spine.set_edgecolor('#ffffff')
            spine.set_linewidth(1.2)

        self.canvas.draw_idle()

    # --- PIP 창 이동 마우스 이벤트 로직 ---
    def on_mouse_press(self, event):
        if event.inaxes == self.ax2 and self.ax2.get_visible():
            # PIP 창 내부를 마우스로 잡은 경우 -> 드래그 시작
            self.is_dragging_pip = True
            self.drag_start_pos = (event.xany if hasattr(event, 'xany') else event.x, event.y)
            # 픽셀 좌표를 Figure 정규화 좌표(0~1)로 변환
            inv = self.fig.transFigure.inverted()
            self.drag_start_fig = inv.transform((event.x, event.y))
            self.pip_bbox_start = list(self.ax2.get_position().bounds)
            return

        if event.inaxes == self.ax1 and event.xdata is not None:
            # 메인 평면을 클릭한 경우 -> PIP 활성화 및 위치 수치 변경
            if not self.ax2.get_visible():
                self.ax2.set_visible(True)

            clicked_x = np.clip(event.xdata, self.x_min, self.x_max)
            self.slider.setValue(int(clicked_x * 100))

    def on_mouse_move(self, event):
        if self.is_dragging_pip and event.x is not None and event.y is not None:
            # 마우스 이동 거리를 계산하여 PIP 좌표 업데이트
            inv = self.fig.transFigure.inverted()
            curr_fig = inv.transform((event.x, event.y))
            
            dx = curr_fig[0] - self.drag_start_fig[0]
            dy = curr_fig[1] - self.drag_start_fig[1]

            new_left = np.clip(self.pip_bbox_start[0] + dx, 0.01, 0.99 - self.pip_bbox_start[2])
            new_bottom = np.clip(self.pip_bbox_start[1] + dy, 0.01, 0.99 - self.pip_bbox_start[3])

            new_bounds = [new_left, new_bottom, self.pip_bbox_start[2], self.pip_bbox_start[3]]
            self.ax2.set_position(new_bounds)
            self.canvas.draw_idle()

    def on_mouse_release(self, event):
        self.is_dragging_pip = False

    def on_slider_change(self, value):
        self.current_a = value / 100.0
        self.label.setText(f"x = {self.current_a:.2f} (PIP 창을 드래그해서 위치를 옮겨보세요)")
        
        if self.current_a not in self.visited_a:
            self.visited_a.append(self.current_a)
            self.visited_a.sort()

        self.update_plot_data()

    def update_plot_data(self):
        if self.current_a is None:
            return

        f_a = f(self.current_a)
        slope = df_numerical(self.current_a)
        tangent_y = slope * (self.x_vals - self.current_a) + f_a

        self.line_tangent.set_data(self.x_vals, tangent_y)
        self.point_tangent.set_data([self.current_a], [f_a])

        if self.ax2.get_visible():
            if len(self.visited_a) > 0:
                visited_x = np.array(self.visited_a)
                visited_y = df_numerical(visited_x)
                self.line_locus.set_data(visited_x, visited_y)

            self.point_slope.set_data([self.current_a], [slope])
            self.v_line.set_xdata([self.current_a])
            self.ax2.set_title(f"Draggable PIP: f'(x) [Slope={slope:.2f}]", fontsize=8, color='white')

        self.canvas.draw_idle()

    def toggle_auto_play(self):
        if self.timer.isActive():
            self.timer.stop()
            self.play_btn.setText("고속 탐색 ▶▶")
        else:
            if not self.ax2.get_visible():
                self.ax2.set_visible(True)
            if self.current_a is None:
                self.slider.setValue(0)
            self.timer.start()
            self.play_btn.setText("일시정지 ❚❚")

    def auto_step(self):
        val = self.slider.value() + (3 * self.sweep_direction)
        if val >= self.slider.maximum() or val <= self.slider.minimum():
            self.sweep_direction *= -1
            val = self.slider.value() + (3 * self.sweep_direction)
        
        self.slider.setValue(val)

# 파이썬 스크립트 실행부
if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = DraggablePIPDerivativeTrackerApp()
    window.show()
    sys.exit(app.exec())