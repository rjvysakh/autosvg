//
// Created by Anuj Kosambi on 21/05/20.
//

#include "CurveUtils.hpp"
#include "underscore.hpp"
#include <NumCpp.hpp>

using namespace std;

void join(const vector<string> &v, char c, string &s) {
    s.clear();
    for (auto p = v.begin();
         p != v.end(); ++p) {
        s += *p;
        if (p != v.end() - 1 && c != NULL)
            s += c;
    }
}


typedef nc::int8 PixelType;

namespace pi {
    class HTMLTag {
    private:
        const char *tagName;
        vector<SVGParam> params;

        string serializeParameters(const vector<SVGParam> &params) {
            string output;
            for (auto param: params) {
                join({output, " ", param.key, "=\"", param.value, "\""}, NULL, output);
            }
            return output;
        }

    public:
        string children = "";

        HTMLTag(const char *tagName, const vector<SVGParam> &params) {
            this->tagName = tagName;
            for (auto param : params) {
                this->params.emplace_back(param);
            }
        }

        string serialize() {
            string output;
            join({"<", this->tagName, " ", this->serializeParameters(this->params)}, NULL, output);

            if (this->children.empty()) {
                join({output, " />"}, NULL, output);
                return output;
            }

            join({output, " >", this->children, "</", this->tagName, ">"}, NULL, output);
            return output;
        }
    };

    class BezierApproximation {
    private:
        static inline PixelType nCr(int n, int r) {
            return fact(n) / (fact(r) * fact(n - r));
        }

        static inline PixelType fact(int n) {
            PixelType res = 1;
            for (PixelType i = 2; i <= n; i++) {
                res = res * i;
            }
            return res;
        }

        template<typename dtype>
        static nc::NdArray<dtype> baseMatrixM(unsigned short n) {
            auto M = nc::zeros<dtype>(n, n);
            for (int i = 0; i < n; i++) {
                M.at(i, i) = BezierApproximation::nCr(n - 1, i);
            }
            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    int sign = (i + j) % 2 == 1 ? -1 : 1;
                    auto value = BezierApproximation::nCr(j, i) * M.at(j, j);
                    M.at(j, i) = (dtype) (sign * value);
                }
            }
            return M;
        }

        static nc::NdArray<double> distanceMatrixS(const nc::NdArray<double> &points) {
            auto length = (nc::uint16) points.shape().rows;
            auto S = nc::zeros<double>(length, 1);
            for (int i = 1; i < length; i++) {
                auto p1X = points.at(i, 0);
                auto p2X = points.at(i - 1, 0);
                auto p1Y = points.at(i, 1);
                auto p2Y = points.at(i - 1, 1);
                S.at(i) = S.at(i - 1) + sqrt(pow((p1X - p2X) * 1.0, 2) + pow((p1Y - p2Y) * 1.0, 2));
            }
            auto perimeter = S.at(length - 1);
            for (int i = 1; i < length; i++) {
                S.at(i) /= perimeter;
            }
            return S;
        }

        static CurveSegment convertCurveSegment(const nc::NdArray<double> &A) {
            CurveSegment segment;
            auto rows = A.shape().rows;
            auto cols = A.shape().cols;
            assert(cols == 2);
            for (int i = 0; i < rows; i++) {
                segment.push_back({int(A.at(i, 0)), int(A.at(i, 1))});
            }
            return segment;
        }

    public:
        static CurveSegment fit(const Contour &contour, unsigned short n) {
            auto length = (nc::uint16) contour.size();
            auto P = nc::zeros<double>(length, 2);
            for (int i = 0; i < length; i++) {
                P.at(i, 0) = (double) contour[i].x;
                P.at(i, 1) = (double) contour[i].y;
            }
            if (n < 3) {
                return BezierApproximation::convertCurveSegment(P);
            }
            auto M = BezierApproximation::baseMatrixM<double>(n);
            auto S = BezierApproximation::distanceMatrixS(P);
            auto T = nc::ones<double>(length, n);
            for (int x = 0; x < T.shape().rows; x++) {
                for (int y = 1; y < n; y++) {
                    T.at(x, y) = pow(S.at(x), y);
                }
            }
            auto Tt = T.transpose();
            auto M1 = nc::linalg::inv<double>(M);
            auto temp = nc::linalg::inv(Tt.dot(T));
            auto A = nc::linalg::multi_dot<double>({M1, temp, Tt, P});
            return BezierApproximation::convertCurveSegment(A);
        }
    };

    vector<Curve>
    CurveUtils::convertContoursToBezierCurves(const vector<Contour> &contours, int sharpness) {
        return underscore::map<vector<Curve>>(contours, [sharpness](Contour contour) -> Curve {
            return CurveUtils::fitContourToCurve(contour, sharpness);
        });
    }

    string CurveUtils::createSvgFromBezierCurves(const vector<Curve> &curves, const vector<Pixel> &colors,
                                                 const vector<SVGParam> &params) {
        HTMLTag svgTag("svg", params);
        struct count {
            int i = 0;
        };
        count counter;
        join(underscore::map<vector<string>>(curves, [&counter, &colors](const Curve &curve) -> string {
            auto i = counter.i;
            string color;
            join({"rgb(", to_string(int(colors[i].x)),
                  ",", to_string(int(colors[i].x)),
                  ",", to_string(int(colors[i].z)), ")"},
                 NULL, color);
            HTMLTag pathTag("path", {
                    {"d",    CurveUtils::convertCurveIntoSvgPathData(curve)},
                    {"fill", color}
            });

            counter.i = counter.i + 1;
            return pathTag.serialize();
        }), '\n', svgTag.children);
        return svgTag.serialize();
    }

    string CurveUtils::convertCurveIntoSvgPathData(const Curve &curve) {
        return underscore::reduce(curve, [](string accum, const CurveSegment segment) -> string {
            string command = accum.empty() ? " M " : " L ";
            const auto isLine = segment.size() < 4;

            auto lineString = underscore::reduce(segment,
                                                 [&isLine, &command, &segment](string line, cv::Point point) -> string {
                                                     string data;
                                                     if (isLine || line.empty()) {
                                                         join({data, line, command,
                                                               to_string(point.x), " ", to_string(point.y)}, NULL,
                                                              data);
                                                     } else if (!isLine && segment[1].x == point.x &&
                                                                segment[1].y == point.y) {
                                                         join({data, line, " C ",
                                                               to_string(point.x), " ", to_string(point.y)}, NULL,
                                                              data);
                                                     } else {
                                                         join({data, line, ", ",
                                                               to_string(point.x), ", ", to_string(point.y)}, NULL,
                                                              data);
                                                     }
                                                     return data;
                                                 }, (string) "");

            string result;

            join({accum, lineString}, '\n', result);

            return result;
        }, (string) "");
    }

    Curve CurveUtils::fitContourToCurve(const Contour &contour, int sharpness) {
        Contour approxCurve;
        auto archLength = cv::arcLength(contour, true);
        auto max_clamp = int(0.001 * archLength * (48 / SHARPNESS));
        auto epsilon = min(max(max_clamp, 1), 10);
        cv::approxPolyDP(contour, approxCurve, epsilon, true);

        Curve output;

        int current_point_index = 0;
        int previous_point_index = 0;

        Contour partition;
        for (int i = 0; i < contour.size(); i++) {
            auto p1 = contour[i];
            if (partition.empty()) {
                partition.push_back(p1);
                continue;
            }
            previous_point_index = current_point_index;
            for (int j = current_point_index; j < approxCurve.size(); j++) {
                if (approxCurve[j].x == p1.x && approxCurve[j].y == p1.y) {
                    current_point_index = j;
                    break;
                }
            }

            if (
                    current_point_index != previous_point_index or
                    (partition.size() > 1 and i == contour.size() - 1)) {
                auto C = CurveUtils::fitPointsToCurveSegment(partition);
                current_point_index = 0;
                previous_point_index = current_point_index;
                partition.clear();
                output.push_back(C);
            } else {
                partition.push_back(p1);
            }
        }
        return output;
    }

    CurveSegment CurveUtils::fitPointsToCurveSegment(const Contour &contour) {
        unsigned short n = min((unsigned short) 4, (unsigned short) contour.size());
        return BezierApproximation::fit(contour, n);
    }
}