#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cwchar>

class ConsoleTable {
public:
	enum class Alignment { Left, Center, Right };

	void addColumn(const std::wstring& name, Alignment align = Alignment::Left) {
		headers_.push_back(name);
		alignments_.push_back(align);
		column_count_++;
	}

	void addRow(const std::vector<std::wstring>& row) {
		if (row.size() != column_count_) {
			throw std::invalid_argument("行数据列数与表头列数不一致");
		}
		rows_.push_back(row);
	}

	template <typename... Args>
	void addRow(Args... args) {
		std::vector<std::wstring> row;
		(row.push_back(toString(args)), ...);
		addRow(row);
	}

	void print(std::wostream& out = std::wcout) const {
		if (column_count_ == 0) return;

		std::vector<size_t> widths(column_count_, 0);
		for (size_t i = 0; i < column_count_; ++i) {
			widths[i] = displayWidth(headers_[i]);
			for (const auto& row : rows_) {
				widths[i] = (std::max)(widths[i], displayWidth(row[i]));
			}
		}

		printLine(out, widths);
		printRow(out, headers_, widths, alignments_);
		printLine(out, widths);
		for (const auto& row : rows_) {
			printRow(out, row, widths, alignments_);
		}
		printLine(out, widths);
	}

private:
	size_t column_count_ = 0;
	std::vector<std::wstring> headers_;
	std::vector<std::vector<std::wstring>> rows_;
	std::vector<Alignment> alignments_;

	static size_t displayWidth(const std::wstring& str) {
		size_t width = 0;
		for (wchar_t wc : str) {
			width += charWidth(wc);
		}
		return width;
	}

	static int charWidth(wchar_t wc) {
		if ((wc >= 0x1100 && wc <= 0x115F) ||   // 韩文辅音
			(wc >= 0x2E80 && wc <= 0xA4CF) ||   // 中日韩部首、汉字等
			(wc >= 0xAC00 && wc <= 0xD7A3) ||   // 韩文音节
			(wc >= 0xF900 && wc <= 0xFAFF) ||   // 中日韩兼容表意文字
			(wc >= 0xFE30 && wc <= 0xFE4F) ||   // 中日韩兼容形式
			(wc >= 0xFF00 && wc <= 0xFF60) ||   // 全角ASCII、全角标点
			(wc >= 0xFFE0 && wc <= 0xFFE6)) {
			return 2;
		}
		return 1;
	}

	template <typename T>
	static std::wstring toString(const T& value) {
		std::wostringstream woss;
		woss << value;
		return woss.str();
	}

	void printCell(std::wostream& out, const std::wstring& content, size_t targetWidth, Alignment align) const {
		size_t contentWidth = displayWidth(content);
		size_t padding = targetWidth > contentWidth ? targetWidth - contentWidth : 0;
		size_t leftPad = 0, rightPad = 0;

		switch (align) {
		case Alignment::Left:
			rightPad = padding;
			break;
		case Alignment::Center:
			leftPad = padding / 2;
			rightPad = padding - leftPad;
			break;
		case Alignment::Right:
			leftPad = padding;
			break;
		}

		out << L"| ";
		out << std::wstring(leftPad, L' ') << content << std::wstring(rightPad, L' ');
	}

	void printRow(std::wostream& out, const std::vector<std::wstring>& row,
		const std::vector<size_t>& widths,
		const std::vector<Alignment>& aligns) const {
		for (size_t i = 0; i < column_count_; ++i) {
			printCell(out, row[i], widths[i], aligns[i]);
		}
		out << L"|\n";
	}

	void printLine(std::wostream& out, const std::vector<size_t>& widths) const {
		out << L"+";
		for (size_t w : widths) {
			out << std::wstring(w + 1, L'-') << L"+";
		}
		out << L"\n";
	}
};