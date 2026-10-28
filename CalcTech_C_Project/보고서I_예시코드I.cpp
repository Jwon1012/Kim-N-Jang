#include "pch.h"
#include <stdio.h>
#include <math.h>

// C-Python 연동을 위한 함수 내보내기 (windows/linux호환)
#ifndef EXPORT
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
#endif


//1. sin(x) 좌표 데이터 생성 함수
//※반환형을 EXPORT로 하면 다른 OS에서도 오류없이 작동함
EXPORT void generate_sin_data(double start, double end, int n_points, double* x_out, double* y_out)
{
	double step = (end - start) / (n_points - 1);
	for (int i = 0; i < n_points; i++)
	{
		x_out[i] = start + (i * step);
		y_out[i] = sin(x_out[i]); //math.h의 sin()연산
	}
}

//2. 특정 x 좌표에서의 sin(x)값과 미분계수 연산 함수
EXPORT void calc_tangent(double x, double* y_out, double* slope_out)
{
	*y_out = sin(x);
	*slope_out = cos(x); //sin(x)의 미분 -> cos(x)
}