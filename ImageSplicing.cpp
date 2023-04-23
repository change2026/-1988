#include<iostream>
#include<opencv2/opencv.hpp>
#include<opencv2/stitching.hpp>
#include<opencv2/xfeatures2d/xfeatures2d.hpp>

using namespace std;
using namespace cv;
using namespace cv::xfeatures2d;
//1��ʹ����������㷨�ҵ�����ͼ�������Ƶĵ㣬����任����
//2����ͼ��right͸�ӱ任��õ���ͼƬ��ͼ��leftƴ��


//ͼ���ںϣ�����ƴ�ӷ�
bool OptimizeSeam(int start_x, int end_x, Mat& WarpImg, Mat& DstImg)
{
	double Width = (end_x - start_x);//�ص�����Ŀ��  

	//ͼ���Ȩ�ںϣ�ͨ���ı�alpha�޸�DstImg��WarpImg����Ȩ�أ��ﵽ�ں�Ч��
	double alpha = 1.0;
	for (int i = 0; i < DstImg.rows; i++)
	{
		for (int j = start_x; j < end_x; j++)
		{
			for (int c = 0; c < 3; c++)
			{
				//���ͼ��WarpImg����Ϊ0������ȫ����DstImg
				if (WarpImg.at<Vec3b>(i, j)[c] == 0)
				{
					alpha = 1.0;
				}
				else
				{
					double l = Width - (j - start_x); //�ص�������ĳһ���ص㵽ƴ�ӷ�ľ���
					alpha = l / Width;
				}
				DstImg.at<Vec3b>(i, j)[c] = DstImg.at<Vec3b>(i, j)[c] * alpha + WarpImg.at<Vec3b>(i, j)[c] * (1.0 - alpha);
			}
		}
	}

	return true;
}


bool Image_Stitching(Mat image_left, Mat image_right, Mat & WarpImg, Mat &DstImg, bool draw)
{
	//����SURF���������
	int Hessian = 800;
	Ptr<SURF>detector = SURF::create(Hessian);

	//����ͼ��������⡢��������
	vector<KeyPoint>keypoint_left, keypoint_right;
	Mat descriptor_left, descriptor_right;
	detector->detectAndCompute(image_left, Mat(), keypoint_left, descriptor_left);
	detector->detectAndCompute(image_right, Mat(), keypoint_right, descriptor_right);

	//ʹ��FLANN�㷨�������������ӵ�ƥ��
	FlannBasedMatcher matcher;
	vector<DMatch>matches;
	matcher.match(descriptor_left, descriptor_right, matches);

	double Max = 0.0;
	for (int i = 0; i < matches.size(); i++)
	{
		//float distance �C>������һ��ƥ�������������������������������ŷ�Ͼ��룬��ֵԽСҲ��˵������������Խ����
		double dis = matches[i].distance;
		if (dis > Max)
		{
			Max = dis;
		}
	}

	//ɸѡ��ƥ��̶ȸߵĹؼ���
	vector<DMatch>goodmatches;
	vector<Point2f>goodkeypoint_left, goodkeypoint_right;
	for (int i = 0; i < matches.size(); i++)
	{
		double dis = matches[i].distance;
		if (dis < 0.15*Max)
		{
			/*
			����ͼ��͸�ӱ任
			��ͼ->queryIdx:��ѯ����������ѯͼ��
			��ͼ->trainIdx:����ѯ��������Ŀ��ͼ��
			*/
			//ע����image_rightͼ����͸�ӱ任����goodkeypoint_left��ӦqueryIdx��goodkeypoint_right��ӦtrainIdx
			//int queryIdx �C>�ǲ���ͼ�����������������descriptor�����±꣬ͬʱҲ����������Ӧ�����㣨keypoint)���±ꡣ
			goodkeypoint_left.push_back(keypoint_left[matches[i].queryIdx].pt);
			//int trainIdx �C> ������ͼ������������������±꣬ͬ��Ҳ����Ӧ����������±ꡣ
			goodkeypoint_right.push_back(keypoint_right[matches[i].trainIdx].pt);
			goodmatches.push_back(matches[i]);
		}
	}

	//����������
	if (draw)
	{
		Mat result;
		drawMatches(image_left, keypoint_left, image_right, keypoint_right, goodmatches, result);
		imshow("����ƥ��", result);

		Mat temp_left = image_left.clone();
		for (int i = 0; i < goodkeypoint_left.size(); i++)
		{
			circle(temp_left, goodkeypoint_left[i], 3, Scalar(0, 255, 0), -1);
		}
		imshow("goodkeypoint_left", temp_left);

		Mat temp_right = image_right.clone();
		for (int i = 0; i < goodkeypoint_right.size(); i++)
		{
			circle(temp_right, goodkeypoint_right[i], 3, Scalar(0, 255, 0), -1);
		}
		imshow("goodkeypoint_right", temp_right);
	}

	//findHomography���㵥Ӧ�Ծ���������Ҫ4����
	/*
	��������ά���֮������ŵ�ӳ��任����H��3x3����ʹ��MSE��RANSAC�������ҵ���ƽ��֮��ı任����
	*/
	if (goodkeypoint_left.size() < 4 || goodkeypoint_right.size() < 4) return false;


	//��ȡͼ��right��ͼ��left��ͶӰӳ����󣬳ߴ�Ϊ3*3
	//ע��˳��srcPoints��Ӧgoodkeypoint_right��dstPoints��Ӧgoodkeypoint_left
	Mat H = findHomography(goodkeypoint_right, goodkeypoint_left, RANSAC);

	//��image_right����͸�ӱ任
	warpPerspective(image_right, WarpImg, H, Size(image_right.cols + image_left.cols, image_right.rows));

	namedWindow("͸�ӱ任", WINDOW_NORMAL);
	imshow("͸�ӱ任", WarpImg);

	DstImg = WarpImg.clone();
	//��image_left������͸�ӱ任���ͼƬ�ϣ����ͼ��ƴ��
	image_left.copyTo(DstImg(Rect(0, 0, image_left.cols, image_left.rows)));
	namedWindow("ͼ��ȫ��ƴ��", WINDOW_NORMAL);
	imshow("ͼ��ȫ��ƴ��", DstImg);

	//͸�ӱ任���Ͻ�(0,0,1)
	Mat V2 = (Mat_<double>(3, 1) << 0.0, 0.0, 1.0);
	Mat V1 = H * V2;
	Point left_top;
	left_top.x = V1.at<double>(0, 0) / V1.at<double>(2, 0);
	left_top.y = V1.at<double>(0, 1) / V1.at<double>(2, 0);
	if (left_top.x < 0)left_top.x = 0;

	//͸�ӱ任���½�(0,src.rows,1)
	V2 = (Mat_<double>(3, 1) << 0.0, image_left.rows, 1.0);
	V1 = H * V2;
	Point left_bottom;
	left_bottom.x = V1.at<double>(0, 0) / V1.at<double>(2, 0);
	left_bottom.y = V1.at<double>(0, 1) / V1.at<double>(2, 0);
	if (left_bottom.x < 0)left_bottom.x = 0;

	int start_x = min(left_top.x, left_bottom.x);//�غ��������
	int end_x = image_left.cols;//�غ������յ�

	OptimizeSeam(start_x, end_x, WarpImg, DstImg); //ͼ���ں�

	namedWindow("ͼ���ں�", WINDOW_NORMAL);
	imshow("ͼ���ں�", DstImg);


	return true;
}


bool OpenCV_Stitching(Mat image_left, Mat image_right)
{
	//����ƴ��ͼƬ�Ž���������
	vector<Mat>images;
	images.push_back(image_left);
	images.push_back(image_right);

	//����Stitcherģ��
	Ptr<Stitcher>stitcher = Stitcher::create();

	Mat result;
	Stitcher::Status status = stitcher->stitch(images, result);// ʹ��stitch��������ƴ��

	if (status != Stitcher::OK) return false;

	imshow("OpenCVͼ��ȫ��ƴ��", result);

	return true;
}

int main()
{

	Mat image_left = imread("D:/Image/IMAGE/opencv_tutorial_data-master/images/change/11.jpg");
	Mat image_right = imread("D:/Image/IMAGE/opencv_tutorial_data-master/images/change/22.jpg");
	if (image_left.empty() || image_right.empty())
	{
		cout << "No Image!" << endl;
		system("pause");
		return -1;
	}

	Mat WarpImg, DstImg;
	if (!Image_Stitching(image_left, image_right, WarpImg, DstImg, false))
	{
		cout << "can not stitching the image!" << endl;
		system("pause");
		return false;
	}


	if (!OpenCV_Stitching(image_left, image_right))
	{
		cout << "can not stitching the image!" << endl;
		system("pause");
		return false;
	}

	waitKey(0);
	destroyAllWindows();
	system("pause");
	return 0;
}
