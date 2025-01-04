close all;
folder = "11";
data = importdata("./" + folder + "/" + folder + ".txt");

duration_s = diff(data.data(:,1)/1000000);
plot(1./duration_s);
hold on;
plot(movmean(1./duration_s,10));

ylabel("Fps");
xlabel("Frame No");